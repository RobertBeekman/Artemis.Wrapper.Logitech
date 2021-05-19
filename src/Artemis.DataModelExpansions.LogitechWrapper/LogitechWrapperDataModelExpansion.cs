using Artemis.Core.DataModelExpansions;
using Artemis.DataModelExpansions.LogitechWrapper.DataModels;
using Artemis.DataModelExpansions.LogitechWrapper.Services;
using Serilog;
using SkiaSharp;
using System;

namespace Artemis.DataModelExpansions.LogitechWrapper
{
    public class LogitechWrapperDataModelExpansion : DataModelExpansion<LogitechWrapperDataModel>
    {
        private readonly ILogger _logger;
        private readonly LogitechWrapperListenerService _wrapperService;

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

        public override void Update(double deltaTime)
        {
        }

        private void WrapperServiceOnBitmapChanged(object sender, EventArgs e)
        {
            foreach (System.Collections.Generic.KeyValuePair<RGB.NET.Core.LedId, SKColor> item in _wrapperService.Colors)
            {
                if (!DataModel.TryGetDynamicChild<SKColor>(item.Key.ToString(), out DynamicChild<SKColor> dc))
                {
                    dc = DataModel.AddDynamicChild<SKColor>(item.Key.ToString(), default);
                }

                dc.Value = item.Value;
            }
        }
    }
}