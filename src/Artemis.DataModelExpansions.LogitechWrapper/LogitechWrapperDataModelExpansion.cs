using Artemis.Core.DataModelExpansions;
using Artemis.DataModelExpansions.LogitechWrapper.DataModels;
using Artemis.DataModelExpansions.LogitechWrapper.Services;
using Serilog;
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
            for (int i = 0; i < _wrapperService.Bitmap.Length; i++)
            {
                var bm = DataModel.Bitmap.DynamicChild<KeyDataModel>(i.ToString()) ??
                    DataModel.Bitmap.AddDynamicChild(new KeyDataModel(), i.ToString());

                bm.Color = _wrapperService.Bitmap[i];
            }
        }
    }
}