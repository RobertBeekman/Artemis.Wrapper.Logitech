using Artemis.Core.Services;
using Serilog;
using SkiaSharp;
using System;
using System.Collections.Generic;
using System.IO.Pipes;
using System.Linq;
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
        private readonly List<(Task, CancellationTokenSource)> _listeners;

        private const string PIPE_NAME = "Artemis\\Logitech";
        private const int LOGI_LED_BITMAP_WIDTH = 21;
        private const int LOGI_LED_BITMAP_HEIGHT = 6;
        private const int LOGI_LED_BITMAP_SIZE = (LOGI_LED_BITMAP_WIDTH * LOGI_LED_BITMAP_HEIGHT);

        private readonly object bitmapLock = new();
        public SKColor[] Bitmap { get; } = new SKColor[LOGI_LED_BITMAP_SIZE];
        public LogiDeviceType DeviceType { get; private set; }

        public event EventHandler BitmapChanged;

        public LogitechWrapperListenerService(ILogger logger)
        {
            _logger = logger;
            _listeners = new List<(Task, CancellationTokenSource)>();
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

                    var cancellationTokenSource = new CancellationTokenSource();
                    await pipeStream.WaitForConnectionAsync(cancellationTokenSource.Token).ConfigureAwait(false);

                    _logger.Information("Client Connected, starting dedicated read thread...");

                    var task = Task.Factory.StartNew(
                        async () => await ProcessClientThread(pipeStream, cancellationTokenSource.Token).ConfigureAwait(false),
                        TaskCreationOptions.LongRunning);

                    _listeners.Add((task, cancellationTokenSource));
                }
                catch (Exception e)
                {
                    _logger.Error("Error processing client", e);
                }
            }
        }

        private async Task ProcessClientThread(NamedPipeServerStream pipeStream, CancellationToken cancellationToken)
        {
            while (!cancellationToken.IsCancellationRequested && pipeStream.IsConnected)
            {
                //packet structure:
                //4 bytes = packet length
                //4 bytes = command id
                //(total - 8) bytes = whatever other data is left

                var lengthBuffer = new byte[4];
                if (await pipeStream.ReadAsync(lengthBuffer.AsMemory(), cancellationToken) < 4)
                {
                    //log?
                    break;
                }
                var packetLength = BitConverter.ToUInt32(lengthBuffer);

                var packet = new byte[packetLength - sizeof(uint)];
                if (await pipeStream.ReadAsync(packet.AsMemory(), cancellationToken) < packet.Length)
                {
                    //log?
                    break;
                }

                var commandId = (LogitechCommand)BitConverter.ToUInt32(packet, 0);
                //run this in another thread to not block the pipe
                //_ = Task.Run(() => ProcessMessage(commandId, packet.AsSpan(sizeof(uint))));
                ProcessMessage(commandId, packet.AsSpan(sizeof(uint)));
            }
            _logger.Information("Pipe stream disconnected, stopping thread...");

            pipeStream.Close();
            pipeStream.Dispose();
        }

        private void ProcessMessage(LogitechCommand commandId, ReadOnlySpan<byte> span)
        {
            switch (commandId)
            {
                case LogitechCommand.Init:
                    _logger.Information("LogiLedInit: {name}", Encoding.UTF8.GetString(span));
                    break;
                case LogitechCommand.Shutdown:
                    _logger.Information("LogiLedShutdown: {name}", Encoding.UTF8.GetString(span));
                    break;
                case LogitechCommand.LogLine:
                    _logger.Information(Encoding.UTF8.GetString(span));
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

                    if (LedMapping.ScanCoded.TryGetValue(scanCode,  out var idx2))
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
                    _logger.Information("Unknown command id: {commandId}. Array: {span}", commandId, span.ToArray());
                    break;
            }
        }

        public void Dispose()
        {
            _serverLoopCancellationTokenSource.Cancel();
            _serverLoop.Wait();
            _serverLoopCancellationTokenSource.Dispose();

            foreach (var (task, token) in _listeners)
            {
                token.Cancel();
                task.Wait();
                token.Dispose();
            }

            _listeners.Clear();
        }
    }
}
