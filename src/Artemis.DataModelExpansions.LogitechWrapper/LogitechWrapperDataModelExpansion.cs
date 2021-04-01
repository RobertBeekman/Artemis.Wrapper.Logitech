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
            Task.Run(ServerLoop);
        }

        public override void Disable()
        {
            _cancellationTokenSource.Cancel();
        }

        public override void Update(double deltaTime)
        {
        }

        private async Task ServerLoop()
        {
            _logger.Information("Starting server loop");
            while (!_cancellationTokenSource.IsCancellationRequested)
            {
                await ProcessNextClient();
            }
        }

        public void ProcessClientThread(object o)
        {
            NamedPipeServerStream pipeStream = (NamedPipeServerStream)o;

            using var streamreader = new StreamReader(pipeStream);
            while (!_cancellationTokenSource.IsCancellationRequested && pipeStream.IsConnected)
            {
                //pipeStream.Read(new byte[2048], 0, 2048);
                //_logger.Information("read");

                HandleMessage(streamreader.ReadLine());
            }
            _logger.Information("Pipe stream disconnected, stopping thread...");

            pipeStream.Close();
            pipeStream.Dispose();
        }

        private void HandleMessage(string data)
        {
            _logger.Information("Received pipe message : \'{data}\'", data);
        }

        public async Task ProcessNextClient()
        {
            try
            {
                NamedPipeServerStream pipeStream = new NamedPipeServerStream(PIPE_NAME, PipeDirection.In, NamedPipeServerStream.MaxAllowedServerInstances);
                _logger.Information("Started new pipe stream, waiting for client");

                await pipeStream.WaitForConnectionAsync(_cancellationTokenSource.Token);
                _logger.Information("Client Connected, starting dedicated read thread...");

                //Spawn a new thread for each request and continue waiting
                Thread thread = new Thread(ProcessClientThread);
                thread.Start(pipeStream);
            }
            catch (Exception e)
            {
                _logger.Error("Error processing client",e);
            }
        }
    }
}