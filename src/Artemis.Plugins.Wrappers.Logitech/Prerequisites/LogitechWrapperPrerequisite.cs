using Artemis.Core;
using Microsoft.Win32;
using System.Collections.Generic;
using System.IO;

namespace Artemis.Plugins.Wrappers.Logitech.Prerequisites
{
    internal class LogitechWrapperPrerequisite : PluginPrerequisite
    {
        public override string Name => "Logitech wrapper registry patch";

        public override string Description => "This registry patch makes games send their lighting to Artemis instead of LGS or LGHUB";

        public override bool RequiresElevation => false;//lol

        public override List<PluginPrerequisiteAction> InstallActions { get; }

        public override List<PluginPrerequisiteAction> UninstallActions { get; }

        public override bool IsMet()
        {
            //this pre requisite *should* set both 32 and 64 bit keys, so we'll only check one of them :)
            RegistryKey key64 = Registry.LocalMachine.OpenSubKey(REGISTRY_PATH_64);
            if (key64 == null)
            {
                return false;
            }

            string dllPath = key64.GetValue(null)?.ToString();

            if (dllPath == null)
            {
                return false;
            }

            return dllPath.Contains(DLL_NAME);
        }

        public LogitechWrapperPrerequisite(Plugin plugin)
        {
            string wrapperPath64 = Path.Combine(plugin.Directory.FullName, "x64", DLL_NAME);
            string wrapperPath32 = Path.Combine(plugin.Directory.FullName, "x86", DLL_NAME);

            InstallActions = new List<PluginPrerequisiteAction>
            {
                new WriteLogitechRegistryPathAction("Patch 64 bit registry", REGISTRY_PATH_64, wrapperPath64),
                new WriteLogitechRegistryPathAction("Patch 32 bit registry", REGISTRY_PATH_32, wrapperPath32)
            };

            UninstallActions = new List<PluginPrerequisiteAction>
            {

            };
        }

        private const string DLL_NAME = "Artemis.Wrapper.Logitech.dll";
        private const string REGISTRY_PATH_64 = "SOFTWARE\\Classes\\CLSID\\{a6519e67-7632-4375-afdf-caa889744403}\\ServerBinary";
        private const string REGISTRY_PATH_32 = "SOFTWARE\\Classes\\WOW6432Node\\CLSID\\{a6519e67-7632-4375-afdf-caa889744403}\\ServerBinary";
    }
}
