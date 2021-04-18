using Artemis.Core.LayerBrushes;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Artemis.DataModelExpansions.LogitechWrapper.LayerBrushes;

namespace Artemis.DataModelExpansions.LogitechWrapper
{
    public class LogitechWrapperLayerBrushProvider : LayerBrushProvider
    {
        public override void Enable()
        {
            RegisterLayerBrushDescriptor<LogitechWrapperLayerBrush>("Logitech Grabber Layer", "Allows you to have Logitech Lightsync lighting on all devices.", "Robber");
        }

        public override void Disable()
        {
        }
    }
}
