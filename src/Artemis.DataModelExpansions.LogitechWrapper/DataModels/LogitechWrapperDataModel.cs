using Artemis.Core.DataModelExpansions;
using SkiaSharp;
using System.Collections.Generic;

namespace Artemis.DataModelExpansions.LogitechWrapper.DataModels
{
    public class LogitechWrapperDataModel : DataModel
    {
        public SKColor SetLighting { get; set; }

        public KeysDataModel Bitmap { get; set; } = new KeysDataModel();
    }

    public class KeysDataModel : DataModel
    {

    }

    public class KeyDataModel : DataModel
    {
        public SKColor Color { get; set; }
    }
}