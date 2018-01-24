//========= Copyright Valve Corporation ============//
#define VR_API_EXPORT 1
#include "openvr.h"
#include "ivrclientcore.h"
#include "pathtools_public.h"
#include "sharedlibtools_public.h"
#include "envvartools_public.h"
#include "hmderrors_public.h"
#include "vrpathregistry_public.h"
#include <mutex>

using vr::EVRInitError;
using vr::IVRSystem;
using vr::IVRClientCore;
using vr::VRInitError_None;

namespace vr
{

static void *g_pVRModule = NULL;
static IVRClientCore *g_pHmdSystem = NULL;
static std::recursive_mutex g_mutexSystem;


typedef void* (*VRClientCoreFactoryFn)(const char *pInterfaceName, int *pReturnCode);

static uint32_t g_nVRToken = 0;

uint32_t VR_GetInitToken()
{
       return g_nVRToken;
}

EVRInitError VR_LoadHmdSystemInternal();
void CleanupInternalInterfaces();


uint32_t VR_InitInternal2( EVRInitError *peError, vr::EVRApplicationType eApplicationType, const char *pStartupInfo )
{
       std::lock_guard<std::recursive_mutex> lock( g_mutexSystem );

       EVRInitError err = VR_LoadHmdSystemInternal();
       if ( err == vr::VRInitError_None )
       {
              err = g_pHmdSystem->Init( eApplicationType, pStartupInfo );
       }

       if ( peError )
              *peError = err;

       if ( err != VRInitError_None )
       {
              SharedLib_Unload( g_pVRModule );
              g_pHmdSystem = NULL;
              g_pVRModule = NULL;

              return 0;
       }

       return ++g_nVRToken;
}

VR_INTERFACE uint32_t VR_CALLTYPE VR_InitInternal( EVRInitError *peError, EVRApplicationType eApplicationType );

uint32_t VR_InitInternal( EVRInitError *peError, vr::EVRApplicationType eApplicationType )
{
       return VR_InitInternal2( peError, eApplicationType, nullptr );
}

void VR_ShutdownInternal()
{
       std::lock_guard<std::recursive_mutex> lock( g_mutexSystem );
       
       if ( g_pHmdSystem )
       {
              g_pHmdSystem->Cleanup();
              g_pHmdSystem = NULL;
       }
       if ( g_pVRModule )
       {
              SharedLib_Unload( g_pVRModule );
              g_pVRModule = NULL;
       }

#if !defined( VR_API_PUBLIC )
       CleanupInternalInterfaces();
#endif

       ++g_nVRToken;
}

EVRInitError VR_LoadHmdSystemInternal()
{
       std::string sRuntimePath, sConfigPath, sLogPath;

       bool bReadPathRegistry = CVRPathRegistry_Public::GetPaths( &sRuntimePath, &sConfigPath, &sLogPath, NULL, NULL );
       if( !bReadPathRegistry )
       {
              return vr::VRInitError_Init_PathRegistryNotFound;
       }

       // figure out where we're going to look for vrclient.dll
       // see if the specified path actually exists.
       if( !Path_IsDirectory( sRuntimePath ) )
       {
              return vr::VRInitError_Init_InstallationNotFound;
       }

       // Because we don't have a way to select debug vs. release yet we'll just
       // use debug if it's there
#if defined( LINUX64 )
       std::string sTestPath = Path_Join( sRuntimePath, "bin", PLATSUBDIR );
#else
       std::string sTestPath = Path_Join( sRuntimePath, "bin" );
#endif
       if( !Path_IsDirectory( sTestPath ) )
       {
              return vr::VRInitError_Init_InstallationCorrupt;
       }

#if defined( WIN64 )
       std::string sDLLPath = Path_Join( sTestPath, "vrclient_x64" DYNAMIC_LIB_EXT );
#else
       std::string sDLLPath = Path_Join( sTestPath, "vrclient" DYNAMIC_LIB_EXT );
#endif

       // only look in the override
       void *pMod = SharedLib_Load( sDLLPath.c_str() );
       // nothing more to do if we can't load the DLL
       if( !pMod )
       {
              return vr::VRInitError_Init_VRClientDLLNotFound;
       }

       VRClientCoreFactoryFn fnFactory = ( VRClientCoreFactoryFn )( SharedLib_GetFunction( pMod, "VRClientCoreFactory" ) );
       if( !fnFactory )
       {
              SharedLib_Unload( pMod );
              return vr::VRInitError_Init_FactoryNotFound;
       }

       int nReturnCode = 0;
       g_pHmdSystem = static_cast< IVRClientCore * > ( fnFactory( vr::IVRClientCore_Version, &nReturnCode ) );
       if( !g_pHmdSystem )
       {
              SharedLib_Unload( pMod );
              return vr::VRInitError_Init_InterfaceNotFound;
       }

       g_pVRModule = pMod;
       return VRInitError_None;
}


void *VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError)
{
       std::lock_guard<std::recursive_mutex> lock( g_mutexSystem );

       if (!g_pHmdSystem)
       {
              if (peError)
                     *peError = vr::VRInitError_Init_NotInitialized;
              return NULL;
       }

       return g_pHmdSystem->GetGenericInterface(pchInterfaceVersion, peError);
}

bool VR_IsInterfaceVersionValid(const char *pchInterfaceVersion)
{
       std::lock_guard<std::recursive_mutex> lock( g_mutexSystem );

       if (!g_pHmdSystem)
       {
              return false;
       }

       return g_pHmdSystem->IsInterfaceVersionValid(pchInterfaceVersion) == VRInitError_None;
}

bool VR_IsHmdPresent()
{
       std::lock_guard<std::recursive_mutex> lock( g_mutexSystem );

       if( g_pHmdSystem )
       {
              // if we're already initialized, just call through
              return g_pHmdSystem->BIsHmdPresent();
       }
       else
       {
              // otherwise we need to do a bit more work
              EVRInitError err = VR_LoadHmdSystemInternal();
              if( err != VRInitError_None )
                     return false;

              bool bHasHmd = g_pHmdSystem->BIsHmdPresent();

              g_pHmdSystem = NULL;
              SharedLib_Unload( g_pVRModule );
              g_pVRModule = NULL;

              return bHasHmd;
       }
}

/** Returns true if the OpenVR runtime is installed. */
bool VR_IsRuntimeInstalled()
{
       std::lock_guard<std::recursive_mutex> lock( g_mutexSystem );

       if( g_pHmdSystem )
       {
              // if we're already initialized, OpenVR is obviously installed
              return true;
       }
       else
       {
              // otherwise we need to do a bit more work
              std::string sRuntimePath, sConfigPath, sLogPath;

              bool bReadPathRegistry = CVRPathRegistry_Public::GetPaths( &sRuntimePath, &sConfigPath, &sLogPath, NULL, NULL );
              if( !bReadPathRegistry )
              {
                     return false;
              }

              // figure out where we're going to look for vrclient.dll
              // see if the specified path actually exists.
              if( !Path_IsDirectory( sRuntimePath ) )
              {
                     return false;
              }

              // the installation may be corrupt in some way, but it certainly looks installed
              return true;
       }
}


/** Returns where OpenVR runtime is installed. */
const char *VR_RuntimePath()
{
       // otherwise we need to do a bit more work
       static std::string sRuntimePath;
       std::string sConfigPath, sLogPath;

       bool bReadPathRegistry = CVRPathRegistry_Public::GetPaths( &sRuntimePath, &sConfigPath, &sLogPath, NULL, NULL );
       if ( !bReadPathRegistry )
       {
              return nullptr;
       }

       // figure out where we're going to look for vrclient.dll
       // see if the specified path actually exists.
       if ( !Path_IsDirectory( sRuntimePath ) )
       {
              return nullptr;
       }

       return sRuntimePath.c_str();
}


/** Returns the symbol version of an HMD error. */
const char *VR_GetVRInitErrorAsSymbol( EVRInitError error )
{
       std::lock_guard<std::recursive_mutex> lock( g_mutexSystem );

       if( g_pHmdSystem )
              return g_pHmdSystem->GetIDForVRInitError( error );
       else
              return GetIDForVRInitError( error );
}


/** Returns the english string version of an HMD error. */
const char *VR_GetVRInitErrorAsEnglishDescription( EVRInitError error )
{
       std::lock_guard<std::recursive_mutex> lock( g_mutexSystem );

       if ( g_pHmdSystem )
              return g_pHmdSystem->GetEnglishStringForHmdError( error );
       else
              return GetEnglishStringForHmdError( error );
}


VR_INTERFACE const char *VR_CALLTYPE VR_GetStringForHmdError( vr::EVRInitError error );

/** Returns the english string version of an HMD error. */
const char *VR_GetStringForHmdError( EVRInitError error )
{
       return VR_GetVRInitErrorAsEnglishDescription( error );
}

}

