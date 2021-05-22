using Artemis.Core.DataModelExpansions;
using Artemis.Plugins.Wrappers.Logitech.DataModelExpansion.DataModels;
using Artemis.Plugins.Wrappers.Logitech.Services;
using RGB.NET.Core;
using Serilog;
using SkiaSharp;
using System;
using System.Collections.Generic;

namespace Artemis.Plugins.Wrappers.Logitech.DataModelExpansion
{
    public class LogitechWrapperDataModelExpansion : DataModelExpansion<LogitechWrapperDataModel>
    {
        private readonly ILogger _logger;
        private readonly LogitechWrapperListenerService _wrapperService;
        private readonly Dictionary<LedId, DynamicChild<SKColor>> _colorsCache = new();

        public LogitechWrapperDataModelExpansion(ILogger logger, LogitechWrapperListenerService service)
        {
            _logger = logger;
            _wrapperService = service;
        }

        public override void Enable()
        {
            _wrapperService.BitmapChanged += WrapperServiceOnBitmapChanged;
        }

        public override void Disable()
        {
            _wrapperService.BitmapChanged -= WrapperServiceOnBitmapChanged;
        }

        public override void Update(double deltaTime) { }

        private void WrapperServiceOnBitmapChanged(object sender, EventArgs e)
        {
            DataModel.BackgroundColor = _wrapperService.BackgroundColor;

            foreach (KeyValuePair<LedId, SKColor> item in _wrapperService.Colors)
            {
                if (!_colorsCache.TryGetValue(item.Key, out DynamicChild<SKColor> colorDataModel))
                {
                    colorDataModel = DataModel.Keys.AddDynamicChild<SKColor>(item.Key.ToString(), default);
                    _colorsCache.Add(item.Key, colorDataModel);
                }

                colorDataModel.Value = item.Value;
            }
        }
    }
}