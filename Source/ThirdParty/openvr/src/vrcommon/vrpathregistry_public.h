//========= Copyright Valve Corporation ============//
#pragma once

#include <string>
#include <vector>
#include <stdint.h>

static const char *k_pchRuntimeOverrideVar = "VR_OVERRIDE";
static const char *k_pchConfigOverrideVar = "VR_CONFIG_PATH";
static const char *k_pchLogOverrideVar = "VR_LOG_PATH";

class CVRPathRegistry_Public
{
public:
       static std::string GetVRPathRegistryFilename();
       static std::string GetOpenVRConfigPath();

public:
       CVRPathRegistry_Public();

       /** Returns paths using the path registry and the provided override values. Pass NULL for any paths you don't care about. 
       * Returns false if the path registry could not be read. Valid paths might still be returned based on environment variables. */
       static bool GetPaths( std::string *psRuntimePath, std::string *psConfigPath, std::string *psLogPath, const char *pchConfigPathOverride, const char *pchLogPathOverride, std::vector<std::string> *pvecExternalDrivers = NULL );

       bool BLoadFromFile();
       bool BSaveToFile() const;

       bool ToJsonString( std::string &sJsonString );

       // methods to get the current values
       std::string GetRuntimePath() const;
       std::string GetConfigPath() const;
       std::string GetLogPath() const;

protected:
       typedef std::vector< std::string > StringVector_t;

       // index 0 is the current setting
       StringVector_t m_vecRuntimePath;
       StringVector_t m_vecLogPath;
       StringVector_t m_vecConfigPath;

       // full list of external drivers
       StringVector_t m_vecExternalDrivers;
};
