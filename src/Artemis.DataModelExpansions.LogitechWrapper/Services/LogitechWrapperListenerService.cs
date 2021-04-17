using Artemis.Core.Services;
using Serilog;
using SkiaSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Artemis.DataModelExpansions.LogitechWrapper.Services
{
    public class LogitechWrapperListenerService : IPluginService, IDisposable
    {
        private readonly ILogger _logger;
        private readonly CancellationTokenSource _serverLoopCancellationTokenSource;
        private readonly Task _serverLoop;
        private readonly List<LogitechWrapperReader> _readers;

        private const string PIPE_NAME = "Artemis\\Logitech";
        private const int LOGI_LED_BITMAP_WIDTH = 21;
        private const int LOGI_LED_BITMAP_HEIGHT = 6;
        private const int LOGI_LED_BITMAP_SIZE = (LOGI_LED_BITMAP_WIDTH * LOGI_LED_BITMAP_HEIGHT);

        private readonly object bitmapLock = new();
        public SKColor[] Bitmap { get; } = new SKColor[LOGI_LED_BITMAP_SIZE];
        public LogiDeviceType DeviceType { get; private set; }

        public event EventHandler BitmapChanged;
        public event EventHandler ClientConnected;

        public LogitechWrapperListenerService(ILogger logger)
        {
            _logger = logger;
            _readers = new List<LogitechWrapperReader>();
            _serverLoopCancellationTokenSource = new CancellationTokenSource();
            _serverLoop = Task.Factory.StartNew(async () => await ServerLoop().ConfigureAwait(false), TaskCreationOptions.LongRunning);
        }

        private async Task ServerLoop()
        {
            _logger.Information("Starting server loop");
            while (!_serverLoopCancellationTokenSource.IsCancellationRequested)
            {
                try
                {
                    NamedPipeServerStream pipeStream = new(
                        PIPE_NAME,
                        PipeDirection.In,
                        NamedPipeServerStream.MaxAllowedServerInstances,
                        PipeTransmissionMode.Byte,
                        PipeOptions.Asynchronous);
                    _logger.Information("Started new pipe stream, waiting for client");

                    await pipeStream.WaitForConnectionAsync(_serverLoopCancellationTokenSource.Token).ConfigureAwait(false);

                    _logger.Information("Client Connected, starting dedicated reader...");

                    var reader = new LogitechWrapperReader(_logger, pipeStream, new CancellationTokenSource());
                    reader.CommandReceived += OnCommandReceived;
                    _readers.Add(reader);

                    ClientConnected?.Invoke(this, EventArgs.Empty);
                }
                catch (Exception e)
                {
                    _logger.Error("Error processing client", e);
                }
            }
        }

        private void OnCommandReceived(object sender, WrapperPacket e)
        {
            ReadOnlySpan<byte> span = e.Packet.Span;
            switch (e.Command)
            {
                case LogitechCommand.Init:
                    _logger.Information("LogiLedInit: {name}", Encoding.UTF8.GetString(span));
                    break;
                case LogitechCommand.Shutdown:
                    _logger.Information("LogiLedShutdown: {name}", Encoding.UTF8.GetString(span));
                    break;
                case LogitechCommand.LogLine:
                    _logger.Information("LogLine: {data}", Encoding.UTF8.GetString(span));
                    break;
                case LogitechCommand.SetTargetDevice:
                    DeviceType = (LogiDeviceType)BitConverter.ToInt32(span);
                    break;
                case LogitechCommand.SetLighting:
                    lock (bitmapLock)
                    {
                        Array.Fill(Bitmap, new SKColor(span[0], span[1], span[2]));
                    }
                    BitmapChanged?.Invoke(this, EventArgs.Empty);
                    break;
                case LogitechCommand.SetLightingForKeyWithKeyName:
                    var keyName = (LogitechLedId)BitConverter.ToInt32(span[3..]);
                    if (LedMapping.BitmapMap.TryGetValue(keyName, out var idx))
                    {
                        lock (bitmapLock)
                        {
                            Bitmap[idx / 4] = new SKColor(span[0], span[1], span[2]);
                        }
                    }

                    BitmapChanged?.Invoke(this, EventArgs.Empty);

                    break;
                case LogitechCommand.SetLightingForKeyWithScanCode:
                    var scanCode = (ScanCode)BitConverter.ToInt32(span[3..]);

                    if (!Enum.IsDefined(scanCode))
                    {
                        _logger.Error("Scancode not defined: {scanCode}", scanCode);
                    }

                    if (LedMapping.ScanCoded.TryGetValue(scanCode, out var idx2))
                    {
                        lock (bitmapLock)
                        {
                            Bitmap[idx2 / 4] = new SKColor(span[0], span[1], span[2]);
                        }
                    }

                    BitmapChanged?.Invoke(this, EventArgs.Empty);

                    break;
                case LogitechCommand.SetLightingFromBitmap:
                    int j = 0;
                    lock (bitmapLock)
                    {
                        for (int i = 0; i < LOGI_LED_BITMAP_SIZE; i += 4)
                        {
                            var color = span.Slice(i, 4);

                            //BGRA
                            Bitmap[j++] = new SKColor(color[2], color[1], color[0], color[3]);
                        }
                    }
                    BitmapChanged?.Invoke(this, EventArgs.Empty);
                    break;
                case LogitechCommand.SaveCurrentLighting:
                    //ignore?
                    break;
                case LogitechCommand.RestoreLighting:
                    //ignore?
                    break;
                default:
                    _logger.Information("Unknown command id: {commandId}.", e.Command);
                    break;
            }
        }

        public void Dispose()
        {
            _serverLoopCancellationTokenSource.Cancel();
            _serverLoop.Wait();
            _serverLoopCancellationTokenSource.Dispose();

            for (int i = _readers.Count - 1; i >= 0; i--)
            {
                _readers[i].CommandReceived -= OnCommandReceived;
                _readers[i].Dispose();
                _readers.RemoveAt(i);
            }

            _readers.Clear();
        }
    }

    internal class LogitechWrapperReader : IDisposable
    {
        private readonly ILogger _logger;
        private readonly NamedPipeServerStream _pipe;
        private readonly CancellationTokenSource _cancellationTokenSource;
        private readonly Task _listenerTask;

        public event EventHandler<WrapperPacket> CommandReceived;

        public LogitechWrapperReader(ILogger logger, NamedPipeServerStream pipe, CancellationTokenSource cancellationTokenSource)
        {
            _logger = logger;
            _pipe = pipe;
            _cancellationTokenSource = cancellationTokenSource;
            _listenerTask = Task.Factory.StartNew(async () => await ReadLoop().ConfigureAwait(false), TaskCreationOptions.LongRunning);
        }

        public async Task<WrapperPacket> ReadWrapperPacket()
        {
            byte[] lengthBuffer = new byte[4];

            if (await _pipe.ReadAsync(lengthBuffer, _cancellationTokenSource.Token) != 4)
                throw new IOException();

            uint packetLength = BitConverter.ToUInt32(lengthBuffer, 0);

            byte[] packet = new byte[packetLength - sizeof(uint)];

            if (await _pipe.ReadAsync(packet, _cancellationTokenSource.Token) != packetLength - sizeof(uint))
                throw new IOException();

            var commandId = (LogitechCommand)BitConverter.ToUInt32(packet, 0);

            return new WrapperPacket(commandId, packet.AsMemory(4));
        }

        private async Task ReadLoop()
        {
            //read and fill in Program Name.
            while (!_cancellationTokenSource.IsCancellationRequested && _pipe.IsConnected)
            {
                var packet = await ReadWrapperPacket();

                CommandReceived?.Invoke(this, packet);
            }
            _logger.Information("Pipe stream disconnected, stopping thread...");
            _pipe.Close();
            await _pipe.DisposeAsync();
        }

        public void Dispose()
        {
            _cancellationTokenSource.Cancel();
            _listenerTask.Wait();
            _listenerTask.Dispose();
            _cancellationTokenSource.Dispose();
        }
    }
}
