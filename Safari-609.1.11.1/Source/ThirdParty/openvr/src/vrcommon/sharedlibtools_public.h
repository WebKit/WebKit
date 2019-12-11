//========= Copyright Valve Corporation ============//
#pragma once

typedef void *SharedLibHandle;

SharedLibHandle SharedLib_Load( const char *pchPath );
void *SharedLib_GetFunction( SharedLibHandle lib, const char *pchFunctionName);
void SharedLib_Unload( SharedLibHandle lib );


