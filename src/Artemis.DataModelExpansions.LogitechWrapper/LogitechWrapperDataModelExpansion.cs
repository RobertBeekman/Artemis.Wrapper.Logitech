using Artemis.Core.DataModelExpansions;
using Artemis.DataModelExpansions.LogitechWrapper.DataModels;
using Serilog;
using System;
using System.IO;
using System.IO.Pipes;
using System.Threading;
using System.Threading.Tasks;
using System.Text;
using SkiaSharp;

namespace Artemis.DataModelExpansions.LogitechWrapper
{
    public class LogitechWrapperDataModelExpansion : DataModelExpansion<LogitechWrapperDataModel>
    {
        private readonly ILogger _logger;
        private const string PIPE_NAME = "Artemis\\Logitech";
        private CancellationTokenSource _cancellationTokenSource;

        public LogitechWrapperDataModelExpansion(ILogger logger)
        {
            _logger = logger;
        }

        public override void Enable()
        {
            _cancellationTokenSource = new CancellationTokenSource();
            var task = new Task(ServerLoop, _cancellationTokenSource.Token, TaskCreationOptions.LongRunning);
            task.Start();
        }

        public override void Disable()
        {
            _cancellationTokenSource.Cancel();
        }

        public override void Update(double deltaTime)
        {
        }

        private async void ServerLoop()
        {
            _logger.Information("Starting server loop");
            while (!_cancellationTokenSource.IsCancellationRequested)
            {
                await ProcessNextClient();
            }
        }

        public async void ProcessClientThread(NamedPipeServerStream pipeStream)
        {
            using var sr = new BinaryReader(pipeStream, Encoding.UTF8);
            while (!_cancellationTokenSource.IsCancellationRequested && pipeStream.IsConnected)
            {
                //packet structure:
                //4 bytes = packet length
                //4 bytes = command id
                //(total - 8) bytes = whatever other data is left

                var lengthBuffer = new byte[4];
                if (await pipeStream.ReadAsync(lengthBuffer.AsMemory(), _cancellationTokenSource.Token) < 4)
                {
                    //log?
                    break;
                }
                var packet = new byte[BitConverter.ToUInt32(lengthBuffer) - sizeof(uint)];
                if (await pipeStream.ReadAsync(packet.AsMemory(), _cancellationTokenSource.Token) < packet.Length)
                {
                    //log?
                    break;
                }
                Task.Run(() => ProcessMessage(BitConverter.ToUInt32(packet, 0), packet.AsSpan(sizeof(uint))));
            }
            _logger.Information("Pipe stream disconnected, stopping thread...");

            pipeStream.Close();
            pipeStream.Dispose();
        }

        private void ProcessMessage(uint commandId, ReadOnlySpan<byte> span)
        {
            switch (commandId)
            {
                case 0:
                    var s = Encoding.UTF8.GetString(span);
                    _logger.Information(s);
                    break;
                case 1:
                    DataModel.SetLighting = new SKColor(span[0], span[1], span[2]);
                    break;
                default:
                    _logger.Information("Unknown command id: {}. Array: {}", commandId, span.ToArray());
                    break;
            }
        }

        public async Task ProcessNextClient()
        {
            try
            {
                NamedPipeServerStream pipeStream = new(
                    PIPE_NAME,
                    PipeDirection.In,
                    NamedPipeServerStream.MaxAllowedServerInstances,
                    PipeTransmissionMode.Byte,
                    PipeOptions.Asynchronous,
                    2048,
                    2048);
                _logger.Information("Started new pipe stream, waiting for client");

                await pipeStream.WaitForConnectionAsync(_cancellationTokenSource.Token);
                _logger.Information("Client Connected, starting dedicated read thread...");

                Task task = new(() => ProcessClientThread(pipeStream), TaskCreationOptions.LongRunning);
                task.Start();
            }
            catch (Exception e)
            {
                _logger.Error("Error processing client", e);
            }
        }
    }
}