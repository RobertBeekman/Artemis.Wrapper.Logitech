using Artemis.Core.DataModelExpansions;
using SkiaSharp;

namespace Artemis.DataModelExpansions.LogitechWrapper.DataModels
{
    public class LogitechWrapperDataModel : DataModel
    {
        public SKColor BackgroundColor { get; set; }
        public KeysDataModel Keys { get; set; } = new();
    }

    public class KeysDataModel : DataModel { }
}