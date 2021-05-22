using Artemis.Core.DataModelExpansions;
using SkiaSharp;

namespace Artemis.DataModelExpansions.LogitechWrapper.DataModelExpansion.DataModels
{
    public class LogitechWrapperDataModel : DataModel
    {
        public SKColor BackgroundColor { get; set; }
        public LogitechKeysDataModel Keys { get; set; } = new();
    }
}