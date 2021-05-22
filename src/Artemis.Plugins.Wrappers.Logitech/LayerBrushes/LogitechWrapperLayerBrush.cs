using Artemis.Core;
using Artemis.Core.LayerBrushes;
using Artemis.DataModelExpansions.LogitechWrapper.Services;
using RGB.NET.Core;
using SkiaSharp;
using System.Collections.Generic;

namespace Artemis.DataModelExpansions.LogitechWrapper.LayerBrushes
{
    public class LogitechWrapperLayerBrush : PerLedLayerBrush<LogitechWrapperLayerPropertyGroup>
    {
        private readonly LogitechWrapperListenerService _wrapperService;
        private readonly Dictionary<LedId, SKColor> _colors = new();

        public LogitechWrapperLayerBrush(LogitechWrapperListenerService wrapperService)
        {
            _wrapperService = wrapperService;
        }

        public override void EnableLayerBrush()
        {
            _wrapperService.BitmapChanged += OnWrapperServiceBitmapChanged;
        }

        public override void DisableLayerBrush()
        {
            _wrapperService.BitmapChanged -= OnWrapperServiceBitmapChanged;
        }

        public override SKColor GetColor(ArtemisLed led, SKPoint renderPoint)
        {
            if (_colors.TryGetValue(led.RgbLed.Id, out SKColor color))
            {
                return color;
            }

            return _wrapperService.BackgroundColor;
        }

        public override void Update(double deltaTime) { }

        private void OnWrapperServiceBitmapChanged(object sender, System.EventArgs e)
        {
            foreach (KeyValuePair<LedId, SKColor> a in _wrapperService.Colors)
            {
                _colors[a.Key] = a.Value;
            }
        }
    }
}
