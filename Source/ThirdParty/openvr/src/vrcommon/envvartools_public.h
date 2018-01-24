//========= Copyright Valve Corporation ============//
#pragma once

#include <string>

std::string GetEnvironmentVariable( const char *pchVarName );
bool SetEnvironmentVariable( const char *pchVarName, const char *pchVarValue );
