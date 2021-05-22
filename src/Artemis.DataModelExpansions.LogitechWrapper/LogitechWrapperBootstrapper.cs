using Artemis.Core;
using Artemis.DataModelExpansions.LogitechWrapper.Prerequisites;

namespace Artemis.DataModelExpansions.LogitechWrapper
{
    public class LogitechWrapperBootstrapper : PluginBootstrapper
    {
        public override void OnPluginLoaded(Plugin plugin)
        {
            AddPluginPrerequisite(new LogitechWrapperPrerequisite(plugin));
        }
    }
}
