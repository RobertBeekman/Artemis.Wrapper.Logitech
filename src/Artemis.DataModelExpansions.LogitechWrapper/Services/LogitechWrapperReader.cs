using Serilog;
using System;
using System.IO;
using System.IO.Pipes;
using System.Threading;
using System.Threading.Tasks;

namespace Artemis.DataModelExpansions.LogitechWrapper.Services
{
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
            _listenerTask = Task.Run(ReadLoop);
        }

        public async Task<WrapperPacket> ReadWrapperPacket()
        {
            byte[] lengthBuffer = new byte[4];

            if (await _pipe.ReadAsync(lengthBuffer, _cancellationTokenSource.Token) != 4)
            {
                throw new IOException();
            }

            uint packetLength = BitConverter.ToUInt32(lengthBuffer, 0);

            byte[] packet = new byte[packetLength - sizeof(uint)];

            if (await _pipe.ReadAsync(packet, _cancellationTokenSource.Token) != packetLength - sizeof(uint))
            {
                throw new IOException();
            }

            LogitechCommand commandId = (LogitechCommand)BitConverter.ToUInt32(packet, 0);

            return new WrapperPacket(commandId, packet.AsMemory(4));
        }

        private async Task ReadLoop()
        {
            //read and fill in Program Name.
            while (!_cancellationTokenSource.IsCancellationRequested && _pipe.IsConnected)
            {
                WrapperPacket packet = await ReadWrapperPacket();

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
