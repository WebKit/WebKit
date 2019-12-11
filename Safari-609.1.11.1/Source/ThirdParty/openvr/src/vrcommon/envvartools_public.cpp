//========= Copyright Valve Corporation ============//
#include "envvartools_public.h"
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>

#undef GetEnvironmentVariable
#undef SetEnvironmentVariable
#endif


std::string GetEnvironmentVariable( const char *pchVarName )
{
#if defined(_WIN32)
       char rchValue[32767]; // max size for an env var on Windows
       DWORD cChars = GetEnvironmentVariableA( pchVarName, rchValue, sizeof( rchValue ) );
       if( cChars == 0 )
              return "";
       else
              return rchValue;
#elif defined(POSIX)
       char *pchValue = getenv( pchVarName );
       if( pchValue )
              return pchValue;
       else
              return "";
#else
#error "Unsupported Platform"
#endif
}


bool SetEnvironmentVariable( const char *pchVarName, const char *pchVarValue )
{
#if defined(_WIN32)
       return 0 != SetEnvironmentVariableA( pchVarName, pchVarValue );
#elif defined(POSIX)
       if( pchVarValue == NULL )
              return 0 == unsetenv( pchVarName );
       else
              return 0 == setenv( pchVarName, pchVarValue, 1 );
#else
#error "Unsupported Platform"
#endif
}
