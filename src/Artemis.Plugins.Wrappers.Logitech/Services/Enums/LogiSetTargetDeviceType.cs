using System;

namespace Artemis.Plugins.Wrappers.Logitech.Services
{

    [Flags]
    public enum LogiSetTargetDeviceType
    {
        LOGI_DEVICETYPE_NONE = 0,
        LOGI_DEVICETYPE_MONOCHROME = 1 << 0,
        LOGI_DEVICETYPE_RGB = 1 << 1,
        LOGI_DEVICETYPE_PERKEY_RGB = 1 << 2,
        LOGI_DEVICETYPE_ALL = LOGI_DEVICETYPE_MONOCHROME | LOGI_DEVICETYPE_RGB | LOGI_DEVICETYPE_PERKEY_RGB
    }
}
