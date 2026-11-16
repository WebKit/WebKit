#ifndef _TCUWAIVERUTIL_HPP
#define _TCUWAIVERUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Waiver mechanism implementation.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include <sstream>
#include <vector>

namespace tcu
{

// Class containing information about session that are printed at the beginning of log.
class SessionInfo
{
public:
    SessionInfo(uint32_t vendorId, uint32_t deviceId, const std::string &deviceName, const std::string &cmdLine);
    SessionInfo(std::string vendor, std::string renderer, const std::string &cmdLine);

    std::string get();

private:
    // WaiverTreeBuilder fills private fields of this class.
    friend class WaiverTreeBuilder;

    // String containing urls to gitlab issues
    // that enable currently used waivers
    std::string m_waiverUrls;

    // String containing command line
    std::string m_cmdLine;

    // Stream containing all info
    std::stringstream m_info;
};

// Class that uses paths to waived tests represented in a form of tree.
// Main functionality of this class is to quickly test test paths in
// order to verify if it is on waived tests list that was read from xml.
class WaiverUtil
{
public:
    WaiverUtil() = default;

    void setup(const std::string waiverFile, std::string packageName, uint32_t vendorId, uint32_t deviceId,
               SessionInfo &sessionInfo);
    void setup(const std::string waiverFile, std::string packageName, std::string vendor, std::string renderer,
               SessionInfo &sessionInfo);

    bool isOnWaiverList(const std::string &casePath) const;

public:
    struct WaiverComponent
    {
        std::string name;
        std::vector<WaiverComponent *> children;
    };

private:
    std::vector<WaiverComponent> m_waiverTree;
};

} // namespace tcu

#endif // _TCUWAIVERUTIL_HPP
