using System;

namespace Artemis.DataModelExpansions.LogitechWrapper.Services
{
    internal class WrapperPacket
    {
        public LogitechCommand Command { get; init; }
        public Memory<byte> Packet { get; init; }

        public WrapperPacket(LogitechCommand command, Memory<byte> packet)
        {
            Command = command;
            Packet = packet;
        }
    }
}
