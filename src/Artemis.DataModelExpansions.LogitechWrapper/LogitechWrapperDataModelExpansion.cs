using Artemis.Core.DataModelExpansions;
using Artemis.DataModelExpansions.LogitechWrapper.DataModels;
using Serilog;
using System;
using System.IO;
using System.IO.Pipes;
using System.Threading;
using System.Threading.Tasks;

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
            using var sr = new StreamReader(pipeStream);
            while (!_cancellationTokenSource.IsCancellationRequested && pipeStream.IsConnected)
            {
                string data = await sr.ReadLineAsync();
                //handle message here. maybe start a new task? any delay here makes the game wait
            }
            _logger.Information("Pipe stream disconnected, stopping thread...");

            pipeStream.Close();
            pipeStream.Dispose();
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