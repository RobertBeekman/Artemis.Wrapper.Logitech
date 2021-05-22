using Artemis.Core;
using Microsoft.Win32;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Artemis.DataModelExpansions.LogitechWrapper.Prerequisites
{
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
            RegistryKey key = Registry.LocalMachine.OpenSubKey(RegistryPath, true);
            //if we can open this key right away, this probably means the user
            //has either LGS or LGHUB installed. we do not need to create the 
            //entire registry structure
            if (key == null)
            {
                key = Registry.LocalMachine;
                foreach (string pathSegment in RegistryPath.Split('\\'))
                {
                    //loop through each path segment, create or open each key
                    //if it fails, throw an exception
                    key = key.CreateSubKey(pathSegment, true);

                    if (key == null)
                    {
                        throw new Exception();
                    }
                }
            }

            string defaultPath = key?.GetValue(null)?.ToString();
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
