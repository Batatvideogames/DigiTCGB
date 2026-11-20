if (-not ('DigiNative.Errors' -as [type])) {
    Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
namespace DigiNative {
    public static class Errors {
        [DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode);
        [DllImport("kernel32.dll")] public static extern uint GetErrorMode();
    }
}
'@
}
