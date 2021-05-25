using Artemis.Core.Services;
using RGB.NET.Core;
using Serilog;
using SkiaSharp;
using System;
using System.Collections.Generic;
using System.IO.Pipes;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Artemis.Plugins.Wrappers.Logitech.Services
{
    public class LogitechWrapperListenerService : IPluginService, IDisposable
    {
        private readonly ILogger _logger;
        private readonly object _lock;
        private readonly List<LogitechWrapperReader> _readers;
        private readonly Dictionary<LedId, SKColor> _colors;
        private readonly List<LedId> _excluded;
        private readonly Task _serverLoop;
        private readonly CancellationTokenSource _serverLoopCancellationTokenSource;

        private const string PIPE_NAME = "Artemis\\Logitech";
        private const int LOGI_LED_BITMAP_WIDTH = 21;
        private const int LOGI_LED_BITMAP_HEIGHT = 6;
        private const int LOGI_LED_BITMAP_BYTES_PER_KEY = 4;

        private const int LOGI_LED_BITMAP_SIZE = (LOGI_LED_BITMAP_WIDTH * LOGI_LED_BITMAP_HEIGHT * LOGI_LED_BITMAP_BYTES_PER_KEY);

        public event EventHandler BitmapChanged;
        public event EventHandler ClientConnected;

        public IReadOnlyDictionary<LedId, SKColor> Colors => _colors;
        public SKColor BackgroundColor { get; private set; }
        public LogiSetTargetDeviceType DeviceType { get; private set; }

        public LogitechWrapperListenerService(ILogger logger)
        {
            _logger = logger;
            _lock = new();
            _colors = new();
            _readers = new();
            _excluded = new();
            _serverLoopCancellationTokenSource = new();
            _serverLoop = Task.Run(ServerLoop);
        }

        private async Task ServerLoop()
        {
            _logger.Information("Starting server loop");
            while (!_serverLoopCancellationTokenSource.IsCancellationRequested)
            {
                try
                {
                    PipeAccessRule rule = new(new SecurityIdentifier(WellKnownSidType.WorldSid, null), PipeAccessRights.FullControl, AccessControlType.Allow);
                    PipeSecurity pipeSecurity = new();
                    pipeSecurity.SetAccessRule(rule);

                    NamedPipeServerStream pipeStream = NamedPipeServerStreamAcl.Create(
                        PIPE_NAME,
                        PipeDirection.In,
                        NamedPipeServerStream.MaxAllowedServerInstances,
                        PipeTransmissionMode.Byte,
                        PipeOptions.Asynchronous,
                        0,
                        0,
                        pipeSecurity);

                    _logger.Information("Started new pipe stream, waiting for client");

                    await pipeStream.WaitForConnectionAsync(_serverLoopCancellationTokenSource.Token).ConfigureAwait(false);

                    _logger.Information("Client Connected, starting dedicated reader...");

                    LogitechWrapperReader reader = new LogitechWrapperReader(_logger, pipeStream, new CancellationTokenSource());
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
            lock (_lock)
            {
                ReadOnlySpan<byte> span = e.Packet.Span;
                switch (e.Command)
                {
                    case LogitechCommand.Init:
                        _logger.Information("LogiLedInit: {name}", Encoding.UTF8.GetString(span));
                        break;
                    case LogitechCommand.Shutdown:
                        _logger.Information("LogiLedShutdown: {name}", Encoding.UTF8.GetString(span));
                        foreach (LedId key in _colors.Keys)
                        {
                            _colors[key] = SKColors.Empty;
                        }
                        _excluded.Clear();
                        DeviceType = LogiSetTargetDeviceType.All;
                        BackgroundColor = SKColors.Empty;
                        BitmapChanged?.Invoke(this, EventArgs.Empty);
                        break;
                    case LogitechCommand.LogLine:
                        _logger.Information("LogLine: {data}", Encoding.UTF8.GetString(span));
                        break;
                    case LogitechCommand.SetTargetDevice:
                        DeviceType = (LogiSetTargetDeviceType)BitConverter.ToInt32(span);
                        _logger.Verbose("SetTargetDevice: {deviceType} ", DeviceType);
                        break;
                    case LogitechCommand.SetLighting:
                        SKColor color = FromSpan(span);
                        if (DeviceType == LogiSetTargetDeviceType.PerKeyRgb)
                        {
                            foreach (LedId key in _colors.Keys)
                            {
                                _colors[key] = color;
                            }
                        }
                        else
                        {
                            BackgroundColor = color;
                        }

                        _logger.Verbose("SetLighting: {color}", color);
                        BitmapChanged?.Invoke(this, EventArgs.Empty);
                        break;
                    case LogitechCommand.SetLightingForKeyWithKeyName:
                        int keyNameIdx = BitConverter.ToInt32(span);
                        SKColor color2 = FromSpan(span[4..]);
                        LogitechLedId keyName = (LogitechLedId)keyNameIdx;

                        if (LedMapping.LogitechLedIds.TryGetValue(keyName, out LedId idx))
                        {
                            _colors[idx] = color2;
                        }

                        _logger.Verbose("SetLightingForKeyWithKeyName: {keyName} ({keyNameIdx}) - {color}", keyName, keyNameIdx, color2);
                        BitmapChanged?.Invoke(this, EventArgs.Empty);
                        break;
                    case LogitechCommand.SetLightingForKeyWithScanCode:
                        int scanCodeIdx = BitConverter.ToInt32(span);
                        SKColor color3 = FromSpan(span[4..]);
                        DirectInputScanCode scanCode = (DirectInputScanCode)scanCodeIdx;

                        if (LedMapping.DirectInputScanCodes.TryGetValue(scanCode, out LedId idx2))
                        {
                            _colors[idx2] = color3;
                        }

                        _logger.Verbose("SetLightingForKeyWithScanCode: {scanCode} ({scanCodeIdx}) - {color}", scanCode, scanCodeIdx, color3);
                        BitmapChanged?.Invoke(this, EventArgs.Empty);
                        break;
                    case LogitechCommand.SetLightingForKeyWithHidCode:
                        int hidCodeIdx = BitConverter.ToInt32(span);
                        SKColor color4 = FromSpan(span[4..]);
                        HidCode hidCode = (HidCode)hidCodeIdx;

                        if (LedMapping.HidCodes.TryGetValue(hidCode, out LedId idx3))
                        {
                            _colors[idx3] = color4;
                        }

                        _logger.Verbose("SetLightingForKeyWithHidCode: {hidCode} ({hidCodeIdx}) - {color}", hidCode, hidCodeIdx, color4);
                        BitmapChanged?.Invoke(this, EventArgs.Empty);
                        break;
                    case LogitechCommand.SetLightingFromBitmap:
                        for (int i = 0; i < LOGI_LED_BITMAP_SIZE; i += 4)
                        {
                            ReadOnlySpan<byte> colorBuff = span.Slice(i, 4);
                            if (LedMapping.BitmapMap.TryGetValue(i, out LedId l) && !_excluded.Contains(l))
                            {
                                //BGRA
                                _colors[l] = new SKColor(colorBuff[2], colorBuff[1], colorBuff[0], colorBuff[3]);
                            }
                        }
                        _logger.Verbose("SetLightingFromBitmap");

                        BitmapChanged?.Invoke(this, EventArgs.Empty);
                        break;
                    case LogitechCommand.ExcludeKeysFromBitmap:
                        var excludeCount = BitConverter.ToInt32(span);
                        for (int i = 0; i < excludeCount; i++)
                        {
                            var excludedLogitechLedId = (LogitechLedId)BitConverter.ToInt32(span[(4 + (i * 4))..]);
                            if (!LedMapping.LogitechLedIds.TryGetValue(excludedLogitechLedId, out var excludedLedId))
                                continue;

                            if (!_excluded.Contains(excludedLedId))
                                _excluded.Add(excludedLedId);
                        }
                        break;
                    default:
                        _logger.Information("Unknown command id: {commandId}.", e.Command);
                        break;
                }
            }
        }

        public static SKColor FromSpan(ReadOnlySpan<byte> span)
            => new(span[0], span[1], span[2]);

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
}
