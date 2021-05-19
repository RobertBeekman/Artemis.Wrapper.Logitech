using Artemis.Core;
using Artemis.Core.LayerBrushes;
using Artemis.DataModelExpansions.LogitechWrapper.Services;
using SkiaSharp;

namespace Artemis.DataModelExpansions.LogitechWrapper.LayerBrushes
{
    public class LogitechWrapperLayerBrush : PerLedLayerBrush<LogitechWrapperLayerPropertyGroup>
    {
        private readonly LogitechWrapperListenerService _wrapperService;

        public LogitechWrapperLayerBrush(LogitechWrapperListenerService wrapperService)
        {
            _wrapperService = wrapperService;
        }

        public override void DisableLayerBrush()
        {

        }

        public override void EnableLayerBrush()
        {

        }

        public override SKColor GetColor(ArtemisLed led, SKPoint renderPoint)
        {
            if (_wrapperService.Colors.TryGetValue(led.RgbLed.Id, out SKColor color))
            {
                return color;
            }

            return SKColor.Empty;
        }

        public override void Update(double deltaTime)
        {

        }
    }

    public class LogitechWrapperLayerPropertyGroup : LayerPropertyGroup
    {
        protected override void DisableProperties()
        {
            //throw new NotImplementedException();
        }

        protected override void EnableProperties()
        {
            //throw new NotImplementedException();
        }

        protected override void PopulateDefaults()
        {
            //throw new NotImplementedException();
        }
    }
}
