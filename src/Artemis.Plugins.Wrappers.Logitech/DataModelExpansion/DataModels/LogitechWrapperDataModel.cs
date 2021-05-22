using Artemis.Core.DataModelExpansions;
using SkiaSharp;

namespace Artemis.Plugins.Wrappers.Logitech.DataModelExpansion.DataModels
{
    public class LogitechWrapperDataModel : DataModel
    {
        public SKColor BackgroundColor { get; set; }
        public LogitechKeysDataModel Keys { get; set; } = new();
    }
}