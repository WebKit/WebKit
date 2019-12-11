//========= Copyright Valve Corporation ============//
#pragma once

#include <stdint.h>
#include <string>


#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/stat.h>
#endif


extern bool BCreateDirectoryRecursive( const char *pchPath );
extern bool BCreateDirectory( const char *pchPath );


