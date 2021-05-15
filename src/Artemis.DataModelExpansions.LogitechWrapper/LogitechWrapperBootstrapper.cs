using Artemis.Core;
using Microsoft.Win32;
using Serilog;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Artemis.DataModelExpansions.LogitechWrapper
{
    public class LogitechWrapperBootstrapper : PluginBootstrapper
    {
        public override void OnPluginLoaded(Plugin plugin)
        {
            AddPluginPrerequisite(new LogitechWrapperPrerequisite(plugin));
        }
    }

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
            var key64 = Registry.LocalMachine.OpenSubKey(REGISTRY_PATH_64);
            if (key64 == null)
                return false;

            var dllPath = key64.GetValue(null)?.ToString();

            if (dllPath == null)
                return false;

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

    public class WriteLogitechRegistryPathAction : PluginPrerequisiteAction
    {
        public string RegistryPath { get; }

        public string Value { get; }

        public WriteLogitechRegistryPathAction(string name, string path, string value) : base(name)
        {
            RegistryPath = path;
            Value = value;
        }

        public override Task Execute(CancellationToken cancellationToken)
        {
            var key = Registry.LocalMachine.OpenSubKey(RegistryPath, true);
            //if we can open this key right away, this probably means the user
            //has either LGS or LGHUB installed. we do not need to create the 
            //entire registry structure
            if (key == null)
            {
                key = Registry.LocalMachine;
                foreach (var pathSegment in RegistryPath.Split('\\'))
                {
                    //loop through each path segment, create or open each key
                    //if it fails, throw an exception
                    key = key.CreateSubKey(pathSegment, true);

                    if (key == null)
                        throw new Exception();
                }
            }

            var defaultPath = key.GetValue(null);
            if (defaultPath != null)
            {
                //if we get here, it means the user already has an installation of either LGS or GHUB.
                //because of that, we have to store the original path somewhere.

                //the dll will look for the value with the name Artemis. If it finds it, all the calls 
                //will be sent to the dll at this location, instead.
                key.SetValue("Artemis", defaultPath);
            }

            key.SetValue(null, Value);

            return Task.CompletedTask;
        }
    }
}
