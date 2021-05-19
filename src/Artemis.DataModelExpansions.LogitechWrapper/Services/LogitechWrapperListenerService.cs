﻿using Artemis.Core.Services;
using RGB.NET.Core;
using Serilog;
using SkiaSharp;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO.Pipes;
using System.Security.AccessControl;
using System.Security.Principal;
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

        public LogiDeviceType DeviceType { get; private set; }

        public ConcurrentDictionary<LedId, SKColor> Colors = new();

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
            ReadOnlySpan<byte> span = e.Packet.Span;
            switch (e.Command)
            {
                case LogitechCommand.Init:
                    _logger.Information("LogiLedInit: {name}", Encoding.UTF8.GetString(span));
                    break;
                case LogitechCommand.Shutdown:
                    _logger.Information("LogiLedShutdown: {name}", Encoding.UTF8.GetString(span));
                    foreach (LedId key in LedMapping.BitmapMap.Values)
                    {
                        Colors[key] = SKColors.Empty;
                    }
                    break;
                case LogitechCommand.LogLine:
                    _logger.Information("LogLine: {data}", Encoding.UTF8.GetString(span));
                    break;
                case LogitechCommand.SetTargetDevice:
                    DeviceType = (LogiDeviceType)BitConverter.ToInt32(span);
                    _logger.Verbose("SetTargetDevice: {deviceType} ", DeviceType);
                    break;
                case LogitechCommand.SetLighting:
                    SKColor color = new SKColor(span[0], span[1], span[2]);
                    foreach (LedId key in LedMapping.BitmapMap.Values)
                    {
                        Colors[key] = color;
                    }
                    _logger.Verbose("SetLighting: {color}", color);
                    BitmapChanged?.Invoke(this, EventArgs.Empty);
                    break;
                case LogitechCommand.SetLightingForKeyWithKeyName:
                    SKColor color2 = new SKColor(span[0], span[1], span[2]);
                    int keyNameIdx = BitConverter.ToInt32(span[3..]);
                    LogitechLedId keyName = (LogitechLedId)keyNameIdx;

                    if (LedMapping.LogitechLedIds.TryGetValue(keyName, out LedId idx))
                    {
                        Colors[idx] = color2;
                    }

                    _logger.Verbose("SetLightingForKeyWithKeyName: {keyName} ({keyNameIdx}) - {color}", keyName, keyNameIdx, color2);
                    BitmapChanged?.Invoke(this, EventArgs.Empty);
                    break;
                case LogitechCommand.SetLightingForKeyWithScanCode:
                    SKColor color3 = new SKColor(span[0], span[1], span[2]);
                    int scanCodeIdx = BitConverter.ToInt32(span[3..]);
                    DirectInputScanCode scanCode = (DirectInputScanCode)scanCodeIdx;

                    if (LedMapping.DirectInputScanCodes.TryGetValue(scanCode, out LedId idx2))
                    {
                        Colors[idx2] = color3;
                    }

                    _logger.Verbose("SetLightingForKeyWithScanCode: {scanCode} ({scanCodeIdx}) - {color}", scanCode, scanCodeIdx, color3);
                    BitmapChanged?.Invoke(this, EventArgs.Empty);
                    break;
                case LogitechCommand.SetLightingForKeyWithHidCode:
                    SKColor color4 = new SKColor(span[0], span[1], span[2]);
                    int hidCodeIdx = BitConverter.ToInt32(span[3..]);
                    HidCode hidCode = (HidCode)hidCodeIdx;

                    if (LedMapping.HidCodes.TryGetValue(hidCode, out LedId idx3))
                    {
                        Colors[idx3] = color4;
                    }

                    _logger.Verbose("SetLightingForKeyWithHidCode: {hidCode} ({hidCodeIdx}) - {color}", hidCode, hidCodeIdx, color4);
                    BitmapChanged?.Invoke(this, EventArgs.Empty);
                    break;
                case LogitechCommand.SetLightingFromBitmap:
                    for (int i = 0; i < LOGI_LED_BITMAP_SIZE; i += 4)
                    {
                        ReadOnlySpan<byte> colorBuff = span.Slice(i, 4);
                        if (LedMapping.BitmapMap.TryGetValue(i, out LedId l))
                        {
                            //BGRA
                            Colors[l] = new SKColor(colorBuff[2], colorBuff[1], colorBuff[0], colorBuff[3]);
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
}
