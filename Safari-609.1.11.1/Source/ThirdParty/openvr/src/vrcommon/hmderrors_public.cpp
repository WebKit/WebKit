//========= Copyright Valve Corporation ============//
#include "openvr.h"
#include "hmderrors_public.h"
#include <stdio.h>
#include <algorithm>

using namespace vr;

#define RETURN_ENUM_AS_STRING(enumValue) case enumValue: return #enumValue;


const char *GetEnglishStringForHmdError( vr::EVRInitError eError )
{
       switch( eError )
       {
       case VRInitError_None:                                                        return "No Error (0)";

       case VRInitError_Init_InstallationNotFound:                     return "Installation Not Found (100)";
       case VRInitError_Init_InstallationCorrupt:                     return "Installation Corrupt (101)";
       case VRInitError_Init_VRClientDLLNotFound:                     return "vrclient Shared Lib Not Found (102)";
       case VRInitError_Init_FileNotFound:                                   return "File Not Found (103)";
       case VRInitError_Init_FactoryNotFound:                            return "Factory Function Not Found (104)";
       case VRInitError_Init_InterfaceNotFound:                     return "Interface Not Found (105)";
       case VRInitError_Init_InvalidInterface:                            return "Invalid Interface (106)";
       case VRInitError_Init_UserConfigDirectoryInvalid:       return "User Config Directory Invalid (107)";
       case VRInitError_Init_HmdNotFound:                                   return "Hmd Not Found (108)";
       case VRInitError_Init_NotInitialized:                            return "Not Initialized (109)";
       case VRInitError_Init_PathRegistryNotFound:                     return "Installation path could not be located (110)";
       case VRInitError_Init_NoConfigPath:                                   return "Config path could not be located (111)";
       case VRInitError_Init_NoLogPath:                                   return "Log path could not be located (112)";
       case VRInitError_Init_PathRegistryNotWritable:              return "Unable to write path registry (113)";
       case VRInitError_Init_AppInfoInitFailed:                     return "App info manager init failed (114)";
       case VRInitError_Init_Retry:                                          return "Internal Retry (115)";
       case VRInitError_Init_InitCanceledByUser:                     return "User Canceled Init (116)";
       case VRInitError_Init_AnotherAppLaunching:                     return "Another app was already launching (117)";
       case VRInitError_Init_SettingsInitFailed:                     return "Settings manager init failed (118)";
       case VRInitError_Init_ShuttingDown:                                   return "VR system shutting down (119)";
       case VRInitError_Init_TooManyObjects:                            return "Too many tracked objects (120)";
       case VRInitError_Init_NoServerForBackgroundApp:              return "Not starting vrserver for background app (121)";
       case VRInitError_Init_NotSupportedWithCompositor:       return "The requested interface is incompatible with the compositor and the compositor is running (122)";
       case VRInitError_Init_NotAvailableToUtilityApps:       return "This interface is not available to utility applications (123)";
       case VRInitError_Init_Internal:                                          return "vrserver internal error (124)";
       case VRInitError_Init_HmdDriverIdIsNone:                     return "Hmd DriverId is invalid (125)";
       case VRInitError_Init_HmdNotFoundPresenceFailed:       return "Hmd Not Found Presence Failed (126)";
       case VRInitError_Init_VRMonitorNotFound:                     return "VR Monitor Not Found (127)";
       case VRInitError_Init_VRMonitorStartupFailed:              return "VR Monitor startup failed (128)";
       case VRInitError_Init_LowPowerWatchdogNotSupported: return "Low Power Watchdog Not Supported (129)";
       case VRInitError_Init_InvalidApplicationType:              return "Invalid Application Type (130)";
       case VRInitError_Init_NotAvailableToWatchdogApps:       return "Not available to watchdog apps (131)";
       case VRInitError_Init_WatchdogDisabledInSettings:       return "Watchdog disabled in settings (132)";
       case VRInitError_Init_VRDashboardNotFound:                     return "VR Dashboard Not Found (133)";
       case VRInitError_Init_VRDashboardStartupFailed:              return "VR Dashboard startup failed (134)";
       case VRInitError_Init_VRHomeNotFound:                            return "VR Home Not Found (135)";
       case VRInitError_Init_VRHomeStartupFailed:                     return "VR home startup failed (136)";
       case VRInitError_Init_RebootingBusy:                            return "Rebooting In Progress (137)";
       case VRInitError_Init_FirmwareUpdateBusy:                     return "Firmware Update In Progress (138)";
       case VRInitError_Init_FirmwareRecoveryBusy:                     return "Firmware Recovery In Progress (139)";

       case VRInitError_Driver_Failed:                                                 return "Driver Failed (200)";
       case VRInitError_Driver_Unknown:                                          return "Driver Not Known (201)";
       case VRInitError_Driver_HmdUnknown:                                          return "HMD Not Known (202)";
       case VRInitError_Driver_NotLoaded:                                          return "Driver Not Loaded (203)";
       case VRInitError_Driver_RuntimeOutOfDate:                            return "Driver runtime is out of date (204)";
       case VRInitError_Driver_HmdInUse:                                          return "HMD already in use by another application (205)";
       case VRInitError_Driver_NotCalibrated:                                   return "Device is not calibrated (206)";
       case VRInitError_Driver_CalibrationInvalid:                            return "Device Calibration is invalid (207)";
       case VRInitError_Driver_HmdDisplayNotFound:                            return "HMD detected over USB, but Monitor not found (208)";
       case VRInitError_Driver_TrackedDeviceInterfaceUnknown:       return "Driver Tracked Device Interface unknown (209)";
       // case VRInitError_Driver_HmdDisplayNotFoundAfterFix:       return "HMD detected over USB, but Monitor not found after attempt to fix (210)"; // taken out upon Ben's request: He thinks that there is no need to separate that error from 208
       case VRInitError_Driver_HmdDriverIdOutOfBounds:                     return "Hmd DriverId is our of bounds (211)";
       case VRInitError_Driver_HmdDisplayMirrored:                            return "HMD detected over USB, but Monitor may be mirrored instead of extended (212)";

       case VRInitError_IPC_ServerInitFailed:                                          return "VR Server Init Failed (300)";
       case VRInitError_IPC_ConnectFailed:                                                 return "Connect to VR Server Failed (301)";
       case VRInitError_IPC_SharedStateInitFailed:                                   return "Shared IPC State Init Failed (302)";
       case VRInitError_IPC_CompositorInitFailed:                                   return "Shared IPC Compositor Init Failed (303)";
       case VRInitError_IPC_MutexInitFailed:                                          return "Shared IPC Mutex Init Failed (304)";
       case VRInitError_IPC_Failed:                                                        return "Shared IPC Failed (305)";
       case VRInitError_IPC_CompositorConnectFailed:                            return "Shared IPC Compositor Connect Failed (306)";
       case VRInitError_IPC_CompositorInvalidConnectResponse:              return "Shared IPC Compositor Invalid Connect Response (307)";
       case VRInitError_IPC_ConnectFailedAfterMultipleAttempts:       return "Shared IPC Connect Failed After Multiple Attempts (308)";

       case VRInitError_Compositor_Failed:                                   return "Compositor failed to initialize (400)";
       case VRInitError_Compositor_D3D11HardwareRequired:       return "Compositor failed to find DX11 hardware (401)";
       case VRInitError_Compositor_FirmwareRequiresUpdate:       return "Compositor requires mandatory firmware update (402)";
       case VRInitError_Compositor_OverlayInitFailed:              return "Compositor initialization succeeded, but overlay init failed (403)";
       case VRInitError_Compositor_ScreenshotsInitFailed:       return "Compositor initialization succeeded, but screenshot init failed (404)";
       case VRInitError_Compositor_UnableToCreateDevice:       return "Compositor unable to create graphics device (405)";

       // Oculus
       case VRInitError_VendorSpecific_UnableToConnectToOculusRuntime:       return "Unable to connect to Oculus Runtime (1000)";

       // Lighthouse
       case VRInitError_VendorSpecific_HmdFound_CantOpenDevice:                            return "HMD found, but can not open device (1101)";
       case VRInitError_VendorSpecific_HmdFound_UnableToRequestConfigStart:       return "HMD found, but unable to request config (1102)";
       case VRInitError_VendorSpecific_HmdFound_NoStoredConfig:                            return "HMD found, but no stored config (1103)";
       case VRInitError_VendorSpecific_HmdFound_ConfigFailedSanityCheck:       return "HMD found, but failed configuration check (1113)";
       case VRInitError_VendorSpecific_HmdFound_ConfigTooBig:                                   return "HMD found, but config too big (1104)";
       case VRInitError_VendorSpecific_HmdFound_ConfigTooSmall:                            return "HMD found, but config too small (1105)";
       case VRInitError_VendorSpecific_HmdFound_UnableToInitZLib:                            return "HMD found, but unable to init ZLib (1106)";
       case VRInitError_VendorSpecific_HmdFound_CantReadFirmwareVersion:              return "HMD found, but problems with the data (1107)";
       case VRInitError_VendorSpecific_HmdFound_UnableToSendUserDataStart:              return "HMD found, but problems with the data (1108)";
       case VRInitError_VendorSpecific_HmdFound_UnableToGetUserDataStart:              return "HMD found, but problems with the data (1109)";
       case VRInitError_VendorSpecific_HmdFound_UnableToGetUserDataNext:              return "HMD found, but problems with the data (1110)";
       case VRInitError_VendorSpecific_HmdFound_UserDataAddressRange:                     return "HMD found, but problems with the data (1111)";
       case VRInitError_VendorSpecific_HmdFound_UserDataError:                                   return "HMD found, but problems with the data (1112)";

       case VRInitError_Steam_SteamInstallationNotFound: return "Unable to find Steam installation (2000)";

       default:
              {
                     static char buf[128];
                     sprintf( buf, "Unknown error (%d)", eError );
                     return buf;
              }
       }

}


const char *GetIDForVRInitError( vr::EVRInitError eError )
{
       switch( eError )
       {
              RETURN_ENUM_AS_STRING( VRInitError_None );
              RETURN_ENUM_AS_STRING( VRInitError_Unknown );

              RETURN_ENUM_AS_STRING( VRInitError_Init_InstallationNotFound );
              RETURN_ENUM_AS_STRING( VRInitError_Init_InstallationCorrupt );
              RETURN_ENUM_AS_STRING( VRInitError_Init_VRClientDLLNotFound );
              RETURN_ENUM_AS_STRING( VRInitError_Init_FileNotFound );
              RETURN_ENUM_AS_STRING( VRInitError_Init_FactoryNotFound );
              RETURN_ENUM_AS_STRING( VRInitError_Init_InterfaceNotFound );
              RETURN_ENUM_AS_STRING( VRInitError_Init_InvalidInterface );
              RETURN_ENUM_AS_STRING( VRInitError_Init_UserConfigDirectoryInvalid );
              RETURN_ENUM_AS_STRING( VRInitError_Init_HmdNotFound );
              RETURN_ENUM_AS_STRING( VRInitError_Init_NotInitialized );
              RETURN_ENUM_AS_STRING( VRInitError_Init_PathRegistryNotFound );
              RETURN_ENUM_AS_STRING( VRInitError_Init_NoConfigPath );
              RETURN_ENUM_AS_STRING( VRInitError_Init_NoLogPath );
              RETURN_ENUM_AS_STRING( VRInitError_Init_PathRegistryNotWritable );
              RETURN_ENUM_AS_STRING( VRInitError_Init_AppInfoInitFailed );
              RETURN_ENUM_AS_STRING( VRInitError_Init_Retry );
              RETURN_ENUM_AS_STRING( VRInitError_Init_InitCanceledByUser );
              RETURN_ENUM_AS_STRING( VRInitError_Init_AnotherAppLaunching );
              RETURN_ENUM_AS_STRING( VRInitError_Init_SettingsInitFailed );
              RETURN_ENUM_AS_STRING( VRInitError_Init_ShuttingDown );
              RETURN_ENUM_AS_STRING( VRInitError_Init_TooManyObjects );
              RETURN_ENUM_AS_STRING( VRInitError_Init_NoServerForBackgroundApp );
              RETURN_ENUM_AS_STRING( VRInitError_Init_NotSupportedWithCompositor );
              RETURN_ENUM_AS_STRING( VRInitError_Init_NotAvailableToUtilityApps );
              RETURN_ENUM_AS_STRING( VRInitError_Init_Internal );
              RETURN_ENUM_AS_STRING( VRInitError_Init_VRMonitorNotFound );
              RETURN_ENUM_AS_STRING( VRInitError_Init_VRMonitorStartupFailed );
              RETURN_ENUM_AS_STRING( VRInitError_Init_LowPowerWatchdogNotSupported );
              RETURN_ENUM_AS_STRING( VRInitError_Init_InvalidApplicationType );
              RETURN_ENUM_AS_STRING( VRInitError_Init_NotAvailableToWatchdogApps );
              RETURN_ENUM_AS_STRING( VRInitError_Init_WatchdogDisabledInSettings );
              RETURN_ENUM_AS_STRING( VRInitError_Init_VRDashboardNotFound );
              RETURN_ENUM_AS_STRING( VRInitError_Init_VRDashboardStartupFailed );
              RETURN_ENUM_AS_STRING( VRInitError_Init_VRHomeNotFound );
              RETURN_ENUM_AS_STRING( VRInitError_Init_VRHomeStartupFailed );
              RETURN_ENUM_AS_STRING( VRInitError_Init_RebootingBusy );
              RETURN_ENUM_AS_STRING( VRInitError_Init_FirmwareUpdateBusy );
              RETURN_ENUM_AS_STRING( VRInitError_Init_FirmwareRecoveryBusy );

              RETURN_ENUM_AS_STRING( VRInitError_Init_HmdDriverIdIsNone );
              RETURN_ENUM_AS_STRING( VRInitError_Init_HmdNotFoundPresenceFailed );

              RETURN_ENUM_AS_STRING( VRInitError_Driver_Failed );
              RETURN_ENUM_AS_STRING( VRInitError_Driver_Unknown );
              RETURN_ENUM_AS_STRING( VRInitError_Driver_HmdUnknown);
              RETURN_ENUM_AS_STRING( VRInitError_Driver_NotLoaded);
              RETURN_ENUM_AS_STRING( VRInitError_Driver_RuntimeOutOfDate);
              RETURN_ENUM_AS_STRING( VRInitError_Driver_HmdInUse);
              RETURN_ENUM_AS_STRING( VRInitError_Driver_NotCalibrated);
              RETURN_ENUM_AS_STRING( VRInitError_Driver_CalibrationInvalid);
              RETURN_ENUM_AS_STRING( VRInitError_Driver_HmdDisplayNotFound);
              RETURN_ENUM_AS_STRING( VRInitError_Driver_TrackedDeviceInterfaceUnknown );
              // RETURN_ENUM_AS_STRING( VRInitError_Driver_HmdDisplayNotFoundAfterFix );
              RETURN_ENUM_AS_STRING( VRInitError_Driver_HmdDriverIdOutOfBounds );
              RETURN_ENUM_AS_STRING( VRInitError_Driver_HmdDisplayMirrored );

              RETURN_ENUM_AS_STRING( VRInitError_IPC_ServerInitFailed);
              RETURN_ENUM_AS_STRING( VRInitError_IPC_ConnectFailed);
              RETURN_ENUM_AS_STRING( VRInitError_IPC_SharedStateInitFailed);
              RETURN_ENUM_AS_STRING( VRInitError_IPC_CompositorInitFailed);
              RETURN_ENUM_AS_STRING( VRInitError_IPC_MutexInitFailed);
              RETURN_ENUM_AS_STRING( VRInitError_IPC_Failed);
              RETURN_ENUM_AS_STRING( VRInitError_IPC_CompositorConnectFailed);
              RETURN_ENUM_AS_STRING( VRInitError_IPC_CompositorInvalidConnectResponse);
              RETURN_ENUM_AS_STRING( VRInitError_IPC_ConnectFailedAfterMultipleAttempts);

              RETURN_ENUM_AS_STRING( VRInitError_Compositor_Failed );
              RETURN_ENUM_AS_STRING( VRInitError_Compositor_D3D11HardwareRequired );
              RETURN_ENUM_AS_STRING( VRInitError_Compositor_FirmwareRequiresUpdate );
              RETURN_ENUM_AS_STRING( VRInitError_Compositor_OverlayInitFailed );
              RETURN_ENUM_AS_STRING( VRInitError_Compositor_ScreenshotsInitFailed );
              RETURN_ENUM_AS_STRING( VRInitError_Compositor_UnableToCreateDevice );

              // Oculus
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_UnableToConnectToOculusRuntime);

              // Lighthouse
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_CantOpenDevice);
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_UnableToRequestConfigStart);
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_NoStoredConfig);
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_ConfigFailedSanityCheck );
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_ConfigTooBig );
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_ConfigTooSmall );
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_UnableToInitZLib );
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_CantReadFirmwareVersion );
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_UnableToSendUserDataStart );
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_UnableToGetUserDataStart );
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_UnableToGetUserDataNext );
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_UserDataAddressRange );
              RETURN_ENUM_AS_STRING( VRInitError_VendorSpecific_HmdFound_UserDataError );

              RETURN_ENUM_AS_STRING( VRInitError_Steam_SteamInstallationNotFound );

       default:
              {
                     static char buf[128];
                     sprintf( buf, "Unknown error (%d)", eError );
                     return buf;
              }
       }
}

