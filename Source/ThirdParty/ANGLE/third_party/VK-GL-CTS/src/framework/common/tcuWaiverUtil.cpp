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

#include "tcuWaiverUtil.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "deString.h"
#include "deStringUtil.hpp"
#include "xeXMLParser.hpp"
#include "tcuCommandLine.hpp"
#include "tcuDefs.hpp"

namespace tcu
{

SessionInfo::SessionInfo(uint32_t vendorId, uint32_t deviceId, const std::string &deviceName,
                         const std::string &cmdLine)
    : m_cmdLine(cmdLine)
{
    m_info << std::hex << "#sessionInfo vendorID 0x" << vendorId << "\n"
           << "#sessionInfo deviceID 0x" << deviceId << "\n"
           << "#sessionInfo deviceName " << deviceName << "\n";
}

SessionInfo::SessionInfo(std::string vendor, std::string renderer, const std::string &cmdLine) : m_cmdLine(cmdLine)
{
    m_info << "#sessionInfo vendor \"" << vendor << "\"\n"
           << "#sessionInfo renderer \"" << renderer << "\"\n";
}

std::string SessionInfo::get()
{
    if (!m_waiverUrls.empty())
    {
        m_info << "#sessionInfo waiverUrls \"" << m_waiverUrls << "\"\n";
        m_waiverUrls.clear();
    }
    if (!m_cmdLine.empty())
    {
        m_info << "#sessionInfo commandLineParameters \"" << m_cmdLine << "\"\n";
        m_cmdLine.clear();
    }
    return m_info.str();
}

// Base class for GL and VK waiver tree builders
class WaiverTreeBuilder
{
public:
    typedef WaiverUtil::WaiverComponent WaiverComponent;

public:
    WaiverTreeBuilder(const std::string &waiverFile, const std::string &packageName, const char *vendorTag,
                      const char *deviceTag, SessionInfo &sessionInfo, std::vector<WaiverComponent> &waiverTree);

    virtual ~WaiverTreeBuilder();

    void build(void);

protected:
    // structure representing component during tree construction
    struct BuilComponent
    {
        std::string name;
        uint32_t parentIndex;                // index in allComponents vector
        std::vector<uint32_t> childrenIndex; // index in allComponents vector

        BuilComponent(std::string n, uint32_t p) : name(std::move(n)), parentIndex(p)
        {
        }
    };

    // parse waiver.xml and read list of waived tests defined
    // specificly for current device id and current vendor id
    bool readWaivedTestsFromXML(void);

    // use list of paths to build a temporary tree which
    // consists of BuilComponents that help with tree construction
    void buildTreeFromPathList(void);

    // use temporary tree to create final tree containing
    // only things that are needed during searches
    void constructFinalTree(void);

    // helper methods used to identify if proper waiver for vendor was found
    virtual bool matchVendor(const std::string &vendor) const = 0;

    // helper methods used after waiver for current vendor was found to check
    // if it is defined also for currend deviceId/renderer
    virtual bool matchDevice(const std::string &device) const = 0;

    // helper method used in buildTreeFromPathList; returns index
    // of component having same ancestors as the component specified
    // in the argument or 0 when build tree does not include this component
    uint32_t findComponentInBuildTree(const std::vector<std::string> &pathComponents, uint32_t index) const;

private:
    const std::string &m_waiverFile;
    const std::string &m_packageName;

    const char *m_vendorTag;
    const char *m_deviceTag;

    // helper attributes used during construction
    std::vector<std::string> m_testList;
    std::vector<BuilComponent> m_buildTree;

    // reference to object containing information about used waivers
    SessionInfo &m_sessionInfo;

    // reference to vector containing final tree
    std::vector<WaiverComponent> &m_finalTree;
};

WaiverTreeBuilder::WaiverTreeBuilder(const std::string &waiverFile, const std::string &packageName,
                                     const char *vendorTag, const char *deviceTag, SessionInfo &sessionInfo,
                                     std::vector<WaiverComponent> &waiverTree)
    : m_waiverFile(waiverFile)
    , m_packageName(packageName)
    , m_vendorTag(vendorTag)
    , m_deviceTag(deviceTag)
    , m_sessionInfo(sessionInfo)
    , m_finalTree(waiverTree)
{
}

WaiverTreeBuilder::~WaiverTreeBuilder()
{
}

void WaiverTreeBuilder::build(void)
{
    const bool fileProcessed = readWaivedTestsFromXML();
    buildTreeFromPathList();
    constructFinalTree();

    if (m_testList.empty() && fileProcessed && !m_waiverFile.empty())
    {
        tcu::print("WARNING: No waivers in '%s' match current vendor and device ID. No tests will be waived.\n",
                   m_waiverFile.c_str());
    }
}

bool WaiverTreeBuilder::readWaivedTestsFromXML()
{
    if (m_waiverFile.empty())
        return true;

    std::ifstream iStream(m_waiverFile);
    if (!iStream.is_open())
    {
        tcu::printError("ERROR: Waiver file '%s' could not be opened. No tests will be waived.\n",
                        m_waiverFile.c_str());
        return false;
    }

    // get whole waiver file content
    std::stringstream buffer;
    buffer << iStream.rdbuf();
    std::string wholeContent = buffer.str();

    if (wholeContent.empty())
    {
        tcu::printError("ERROR: Waiver file '%s' is invalid. No tests will be waived.\n", m_waiverFile.c_str());
        return false;
    }

    // feed parser with xml content
    xe::xml::Parser xmlParser;
    xmlParser.feed(reinterpret_cast<const uint8_t *>(wholeContent.c_str()), static_cast<int>(wholeContent.size()));
    xmlParser.advance();

    // first we find matching vendor, then search for matching device/renderer and then memorize cases
    bool vendorFound  = false;
    bool deviceFound  = false;
    bool scanDevice   = false;
    bool memorizeCase = false;
    std::string waiverUrl;
    std::vector<std::string> waiverTestList;

    while (true)
    {
        // we are grabing elements one by one - depth-first traversal in pre-order
        xe::xml::Element currElement = xmlParser.getElement();

        // stop if there is parsing error or we didnt found
        // waiver for current vendor id and device id/renderer
        if (currElement == xe::xml::ELEMENT_INCOMPLETE || currElement == xe::xml::ELEMENT_END_OF_STRING)
            break;

        const char *elemName = xmlParser.getElementName();
        switch (currElement)
        {
        case xe::xml::ELEMENT_START:
            if (vendorFound)
            {
                if (!deviceFound)
                {
                    // if we found proper vendor and are reading deviceIds/rendererers list then allow it
                    scanDevice = strcmp(elemName, m_deviceTag) == 0; // e.g. "d"
                    if (scanDevice)
                        break;
                }

                // if we found waiver for current vendor and are reading test case names then allow it
                memorizeCase = strcmp(elemName, "t") == 0;
                break;
            }

            // we are searching for waiver definition for current vendor, till we find
            // it we skip everythingh; we also skip tags that we don't need eg. description
            if (strcmp(elemName, "waiver") != 0)
                break;

            // we found waiver tag, check if it is deffined for current vendor
            waiverTestList.clear();
            if (xmlParser.hasAttribute(m_vendorTag))
            {
                vendorFound = matchVendor(xmlParser.getAttribute(m_vendorTag));
                // if waiver vendor matches current one then memorize waiver url
                // it will be needed when deviceId/renderer will match current one
                if (vendorFound)
                    waiverUrl = xmlParser.getAttribute("url");
            }
            break;

        case xe::xml::ELEMENT_DATA:
            if (scanDevice)
            {
                // check if device read from xml matches current device/renderer
                std::string waivedDevice;
                xmlParser.getDataStr(waivedDevice);
                deviceFound = matchDevice(waivedDevice);
            }
            else if (memorizeCase)
            {
                // memorize whats betwean <t></t> tags when case name starts with current package name
                // note: waiver tree is constructed per package
                std::string waivedCaseName;
                xmlParser.getDataStr(waivedCaseName);
                if (waivedCaseName.find(m_packageName) == 0)
                    waiverTestList.push_back(waivedCaseName);
            }
            break;

        case xe::xml::ELEMENT_END:
            memorizeCase = false;
            scanDevice   = false;
            if (strcmp(elemName, "waiver") == 0)
            {
                // when we found proper waiver we can copy memorized cases and update waiver info
                if (vendorFound && deviceFound)
                {
                    // each waiver must have a url and at least one test
                    DE_ASSERT(!waiverTestList.empty() && !waiverUrl.empty());

                    std::string &urls = m_sessionInfo.m_waiverUrls;
                    m_testList.insert(m_testList.end(), waiverTestList.begin(), waiverTestList.end());

                    // if m_waiverUrls is not empty then we found another waiver
                    // definition that should be applyed for this device; we need to
                    // add space to urls attribute to separate new url from previous one
                    if (!urls.empty())
                        urls.append(" ");
                    urls.append(waiverUrl);
                }
                vendorFound = false;
                deviceFound = false;
            }
            break;

        default:
            DE_ASSERT(false);
        }

        xmlParser.advance();
    }

    return true;
}

uint32_t WaiverTreeBuilder::findComponentInBuildTree(const std::vector<std::string> &pathComponents,
                                                     uint32_t index) const
{
    const std::string &checkedName = pathComponents[index];

    // check if same component is already in the build tree; we start from 1 - skiping root
    for (uint32_t componentIndex = 1; componentIndex < m_buildTree.size(); ++componentIndex)
    {
        const BuilComponent &componentInTree = m_buildTree[componentIndex];
        if (componentInTree.name != checkedName)
            continue;

        // names match so we need to make sure that all their ancestors match too;
        uint32_t reverseLevel        = index;
        uint32_t ancestorInTreeIndex = componentInTree.parentIndex;

        // if this component and the ancestor component are both the next level after root, we have a match
        if ((reverseLevel == 1) && (ancestorInTreeIndex == 0))
            return componentIndex;

        while (--reverseLevel > 0)
        {
            // names dont match - we can move to searching other build tree items
            if (pathComponents[reverseLevel] != m_buildTree[ancestorInTreeIndex].name)
                break;

            // when previous path component matches ancestor name then we need do check earlier path component
            ancestorInTreeIndex = m_buildTree[ancestorInTreeIndex].parentIndex;

            // we reached root
            if (ancestorInTreeIndex == 0)
            {
                // if next level would be root then ancestors match
                if (reverseLevel == 1)
                    return componentIndex;
                // if next level is not root then ancestors dont match
                break;
            }
        }
    }

    // searched path component is not in the tree
    return 0;
}

void WaiverTreeBuilder::buildTreeFromPathList(void)
{
    if (m_testList.empty())
        return;

    uint32_t parentIndex = 0;

    // construct root node
    m_buildTree.emplace_back("root", 0);

    for (const auto &path : m_testList)
    {
        const std::vector<std::string> pathComponents = de::splitString(path, '.');

        // first component is parented to root
        parentIndex = 0;

        // iterate over all components of current path, but skip first one (e.g. "dEQP-VK", "KHR-GLES31")
        for (uint32_t level = 1; level < pathComponents.size(); ++level)
        {
            // check if same component is already in the tree and we dont need to add it
            uint32_t componentIndex = findComponentInBuildTree(pathComponents, level);
            if (componentIndex)
            {
                parentIndex = componentIndex;
                continue;
            }

            // component is not in the tree, add it
            const std::string componentName = pathComponents[level];
            m_buildTree.emplace_back(componentName, parentIndex);

            // add current component as a child to its parent and assume
            // that this component will be parent of next component
            componentIndex = static_cast<uint32_t>(m_buildTree.size() - 1);
            m_buildTree[parentIndex].childrenIndex.push_back(componentIndex);
            parentIndex = componentIndex;
        }
    }
}

void WaiverTreeBuilder::constructFinalTree(void)
{
    if (m_buildTree.empty())
        return;

    // translate vector of BuilComponents to vector of WaiverComponents
    m_finalTree.resize(m_buildTree.size());
    for (uint32_t i = 0; i < m_finalTree.size(); ++i)
    {
        BuilComponent &buildCmponent     = m_buildTree[i];
        WaiverComponent &waiverComponent = m_finalTree[i];

        waiverComponent.name = std::move(buildCmponent.name);
        waiverComponent.children.resize(buildCmponent.childrenIndex.size());

        // set pointers for children
        for (uint32_t j = 0; j < buildCmponent.childrenIndex.size(); ++j)
        {
            uint32_t childIndexInTree   = buildCmponent.childrenIndex[j];
            waiverComponent.children[j] = &m_finalTree[childIndexInTree];
        }
    }
}

// Class that builds a tree out of waiver definitions for OpenGL tests.
// Most of functionalities are shared betwean VK and GL builders and they
// were extracted to WaiverTreeBuilder base class.
class GLWaiverTreeBuilder : public WaiverTreeBuilder
{
public:
    GLWaiverTreeBuilder(const std::string &waiverFile, const std::string &packageName, const std::string &currentVendor,
                        const std::string &currentRenderer, SessionInfo &sessionInfo,
                        std::vector<WaiverComponent> &waiverTree);

    bool matchVendor(const std::string &vendor) const override;
    bool matchDevice(const std::string &device) const override;

private:
    const std::string m_currentVendor;
    const std::string m_currentRenderer;
};

GLWaiverTreeBuilder::GLWaiverTreeBuilder(const std::string &waiverFile, const std::string &packageName,
                                         const std::string &currentVendor, const std::string &currentRenderer,
                                         SessionInfo &sessionInfo, std::vector<WaiverComponent> &waiverTree)
    : WaiverTreeBuilder(waiverFile, packageName, "vendor", "r", sessionInfo, waiverTree)
    , m_currentVendor(currentVendor)
    , m_currentRenderer(currentRenderer)
{
}

bool GLWaiverTreeBuilder::matchVendor(const std::string &vendor) const
{
    return tcu::matchWildcards(vendor.cbegin(), vendor.cend(), m_currentVendor.cbegin(), m_currentVendor.cend(), false);
}

bool GLWaiverTreeBuilder::matchDevice(const std::string &device) const
{
    // make sure that renderer name in .xml is not within "", those extra characters should be removed
    DE_ASSERT(device[0] != '\"');

    return tcu::matchWildcards(device.cbegin(), device.cend(), m_currentRenderer.cbegin(), m_currentRenderer.cend(),
                               false);
}

// Class that builds a tree out of waiver definitions for Vulkan tests.
// Most of functionalities are shared betwean VK and GL builders and they
// were extracted to WaiverTreeBuilder base class.
class VKWaiverTreeBuilder : public WaiverTreeBuilder
{
public:
    VKWaiverTreeBuilder(const std::string &waiverFile, const std::string &packageName, const uint32_t currentVendor,
                        const uint32_t currentRenderer, SessionInfo &sessionInfo,
                        std::vector<WaiverComponent> &waiverTree);

    bool matchVendor(const std::string &vendor) const override;
    bool matchDevice(const std::string &device) const override;

private:
    const uint32_t m_currentVendorId;
    const uint32_t m_currentDeviceId;
};

VKWaiverTreeBuilder::VKWaiverTreeBuilder(const std::string &waiverFile, const std::string &packageName,
                                         const uint32_t currentVendor, const uint32_t currentRenderer,
                                         SessionInfo &sessionInfo, std::vector<WaiverComponent> &waiverTree)
    : WaiverTreeBuilder(waiverFile, packageName, "vendorId", "d", sessionInfo, waiverTree)
    , m_currentVendorId(currentVendor)
    , m_currentDeviceId(currentRenderer)
{
}

bool VKWaiverTreeBuilder::matchVendor(const std::string &vendor) const
{
    return (m_currentVendorId == static_cast<uint32_t>(std::stoul(vendor, 0, 0)));
}

bool VKWaiverTreeBuilder::matchDevice(const std::string &device) const
{
    return (m_currentDeviceId == static_cast<uint32_t>(std::stoul(device, 0, 0)));
}

void WaiverUtil::setup(const std::string waiverFile, std::string packageName, uint32_t vendorId, uint32_t deviceId,
                       SessionInfo &sessionInfo)
{
    VKWaiverTreeBuilder(waiverFile, packageName, vendorId, deviceId, sessionInfo, m_waiverTree).build();
}

void WaiverUtil::setup(const std::string waiverFile, std::string packageName, std::string vendor, std::string renderer,
                       SessionInfo &sessionInfo)
{
    GLWaiverTreeBuilder(waiverFile, packageName, vendor, renderer, sessionInfo, m_waiverTree).build();
}

bool WaiverUtil::isOnWaiverList(const std::string &casePath) const
{
    if (m_waiverTree.empty())
        return false;

    // skip root e.g. "dEQP-VK"
    size_t firstDotPos                         = casePath.find('.');
    std::string::const_iterator componentStart = casePath.cbegin() + firstDotPos + 1;
    std::string::const_iterator componentEnd   = componentStart;
    std::string::const_iterator pathEnd        = casePath.cend();
    const WaiverComponent *waiverComponent     = m_waiverTree.data();

    // check path component by component
    while (true)
    {
        // find the last character of next component
        ++componentEnd;
        for (; componentEnd < pathEnd; ++componentEnd)
        {
            if (*componentEnd == '.')
                break;
        }

        // check if one of children has the same component name
        for (const auto &c : waiverComponent->children)
        {
            bool matchFound =
                tcu::matchWildcards(c->name.cbegin(), c->name.cend(), componentStart, componentEnd, false);

            // current waiver component matches curent path component - go to next component
            if (matchFound)
            {
                waiverComponent = c;
                break;
            }
        }

        // we checked all components - if our pattern was a leaf then this test should be waived
        if (componentEnd == pathEnd)
            return waiverComponent->children.empty();

        // go to next test path component
        componentStart = componentEnd + 1;
    }
    return false;
}

} // namespace tcu
