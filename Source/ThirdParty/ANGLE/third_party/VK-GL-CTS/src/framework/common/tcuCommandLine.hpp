#ifndef _TCUCOMMANDLINE_HPP
#define _TCUCOMMANDLINE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Command line parsing.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "deCommandLine.hpp"
#include "tcuTestCase.hpp"
#include "deUniquePtr.hpp"

#include <string>
#include <vector>
#include <istream>

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Run mode tells whether the test program should run the tests or
 *          dump out metadata about the tests.
 *//*--------------------------------------------------------------------*/
enum RunMode
{
    RUNMODE_EXECUTE = 0,        //! Test program executes the tests.
    RUNMODE_DUMP_XML_CASELIST,  //! Test program dumps the list of contained test cases in XML format.
    RUNMODE_DUMP_TEXT_CASELIST, //! Test program dumps the list of contained test cases in plain-text format.
    RUNMODE_DUMP_STDOUT_CASELIST, //! Test program dumps the list of contained test cases in plain-text format into stdout.
    RUNMODE_DUMP_TEXT_TRIE,       //! Test program dumps the list of contained test cases in trie format to a text file.
    RUNMODE_DUMP_STDOUT_TRIE,     //! Test program dumps the list of contained test cases in trie format to stdout.
    RUNMODE_VERIFY_AMBER_COHERENCY, //! Test program verifies that amber tests have coherent capability requirements
    RUNMODE_GEN_MUSTPASS,           //! Test program generates per-config mustpass files from a spec.

    RUNMODE_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief Should graphical tests show rendering results on screen.
 *//*--------------------------------------------------------------------*/
enum WindowVisibility
{
    WINDOWVISIBILITY_WINDOWED = 0,
    WINDOWVISIBILITY_FULLSCREEN,
    WINDOWVISIBILITY_HIDDEN,

    WINDOWVISIBILITY_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief The type of rendering surface the tests should be executed on.
 *//*--------------------------------------------------------------------*/
enum SurfaceType
{
    SURFACETYPE_WINDOW = 0,        //!< Native window.
    SURFACETYPE_OFFSCREEN_NATIVE,  //!< Native offscreen surface, such as pixmap.
    SURFACETYPE_OFFSCREEN_GENERIC, //!< Generic offscreen surface, such as pbuffer.
    SURFACETYPE_FBO,               //!< Framebuffer object.

    SURFACETYPE_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief Screen rotation, always to clockwise direction.
 *//*--------------------------------------------------------------------*/
enum ScreenRotation
{
    SCREENROTATION_UNSPECIFIED, //!< Use default / current orientation.
    SCREENROTATION_0,           //!< Set rotation to 0 degrees from baseline.
    SCREENROTATION_90,
    SCREENROTATION_180,
    SCREENROTATION_270,

    SCREENROTATION_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief Dump output of vulkan video decoding tests.
 *//*--------------------------------------------------------------------*/
enum VideoDecodeOutput
{
    DUMP_DEC_DISABLE = 0,       // Disabled by default
    DUMP_DEC_TO_SINGLE,         // Write all decoded frames to one single file
    DUMP_DEC_TO_SEPARATE_FILES, // Write each decoded frame to its own file
    DUMP_DEC_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief Dump output of vulkan video encoding tests.
 *//*--------------------------------------------------------------------*/
enum VideoEncodeOutput
{
    DUMP_ENC_DISABLE = 0, // Disabled by default
    DUMP_ENC_YUV,         // Write input yuv and output yuv from encoded bitstream.
    DUMP_ENC_BITSTREAM,   // Write encoded bitstream.
    DUMP_ENC_ALL,         // Write both yuv and encoded bitstream.
    DUMP_ENC_LAST
};

class CaseTreeNode;
class CasePaths;
class Archive;

// Match a single path component against a pattern component that may contain *-wildcards.
bool matchWildcards(std::string::const_iterator patternStart, std::string::const_iterator patternEnd,
                    std::string::const_iterator pathStart, std::string::const_iterator pathEnd, bool allowPrefix);

class CaseListFilter
{
public:
    CaseListFilter(const de::cmdline::CommandLine &cmdLine, const tcu::Archive &archive);
    CaseListFilter(void);
    ~CaseListFilter(void);

    //! Check if test group is in supplied test case list.
    bool checkTestGroupName(const char *groupName) const;

    //! Check if test case is in supplied test case list.
    bool checkTestCaseName(const char *caseName) const;

    //! Check if test group passes the case fraction filter.
    bool checkCaseFraction(int i, const std::string &testCaseName, bool useFraction0) const;

    //! Check if test case runner is of supplied type
    bool checkRunnerType(tcu::TestRunnerType type) const
    {
        return ((m_runnerType & type) == m_runnerType);
    }

private:
    CaseListFilter(const CaseListFilter &);            // not allowed!
    CaseListFilter &operator=(const CaseListFilter &); // not allowed!

    CaseTreeNode *m_caseTree;
    de::MovePtr<const CasePaths> m_casePaths;
    de::MovePtr<const CasePaths> m_excludePaths;
    std::vector<int> m_caseFraction;
    de::MovePtr<const CasePaths> m_caseFractionMandatoryTests;
    tcu::TestRunnerType m_runnerType;
};

/*--------------------------------------------------------------------*//*!
 * \brief Test command line
 *
 * CommandLine handles argument parsing and provides convinience functions
 * for querying test parameters.
 *//*--------------------------------------------------------------------*/
class CommandLine
{
public:
    CommandLine(void);
    CommandLine(int argc, const char *const *argv);
    explicit CommandLine(const std::string &cmdLine);
    virtual ~CommandLine(void);

    bool parse(int argc, const char *const *argv);
    bool parse(const std::string &cmdLine);

    const std::string &getApplicationName(void) const;
    const std::string &getInitialCmdLine(void) const;

    //! Is quiet mode active?
    bool quietMode(void) const;

    //! Get log file name (--deqp-log-filename)
    const char *getLogFileName(void) const;

    //! Get logging flags
    uint32_t getLogFlags(void) const;

    //! Get run mode (--deqp-runmode)
    RunMode getRunMode(void) const;

    //! Get caselist dump target file pattern (--deqp-caselist-export-file)
    const char *getCaseListExportFile(void) const;

    //! Get path to the mustpass generation spec file (--deqp-mustpass-spec)
    const char *getMustpassSpec(void) const;

    //! Get default window visibility (--deqp-visibility)
    WindowVisibility getVisibility(void) const;

    //! Get watchdog enable status (--deqp-watchdog)
    bool isWatchDogEnabled(void) const;

    //! Get watchdog total test case time limit in seconds (--deqp-watchdog-total-time-limit)
    int getWatchDogTotalTime(void) const;

    //! Get watchdog per iteration time limit in seconds time limit in seconds (--deqp-watchdog-interval-time-limit)
    int getWatchDogIntervalTime(void) const;

    //! Get crash handling enable status (--deqp-crashhandler)
    bool isCrashHandlingEnabled(void) const;

    //! Get base seed for randomization (--deqp-base-seed)
    int getBaseSeed(void) const;

    //! Get test iteration count (--deqp-test-iteration-count)
    int getTestIterationCount(void) const;

    //! Get rendering target width (--deqp-surface-width)
    int getSurfaceWidth(void) const;

    //! Get rendering target height (--deqp-surface-height)
    int getSurfaceHeight(void) const;

    //! Get rendering taget type (--deqp-surface-type)
    SurfaceType getSurfaceType(void) const;

    //! Get screen rotation (--deqp-screen-rotation)
    ScreenRotation getScreenRotation(void) const;

    //! Get GL context factory name (--deqp-gl-context-type)
    const char *getGLContextType(void) const;

    //! Get GL config ID (--deqp-gl-config-id)
    int getGLConfigId(void) const;

    //! Get GL config name (--deqp-gl-config-name)
    const char *getGLConfigName(void) const;

    //! Get GL context flags (--deqp-gl-context-flags)
    const char *getGLContextFlags(void) const;

    //! Get OpenCL platform ID (--deqp-cl-platform-id)
    int getCLPlatformId(void) const;

    //! Get OpenCL device IDs (--deqp-cl-device-ids)
    void getCLDeviceIds(std::vector<int> &deviceIds) const
    {
        deviceIds = getCLDeviceIds();
    }
    const std::vector<int> &getCLDeviceIds(void) const;

    //! Get extra OpenCL program build options (--deqp-cl-build-options)
    const char *getCLBuildOptions(void) const;

    //! Get EGL native display factory (--deqp-egl-display-type)
    const char *getEGLDisplayType(void) const;

    //! Get EGL native window factory (--deqp-egl-window-type)
    const char *getEGLWindowType(void) const;

    //! Get EGL native pixmap factory (--deqp-egl-pixmap-type)
    const char *getEGLPixmapType(void) const;

    //! Get Vulkan device ID (--deqp-vk-device-id)
    int getVKDeviceId(void) const;

    //! Get max custom device count (--deqp-vk-max-custom-devices)
    int getMaxCustomDevices(void) const;

    //! Get Vulkan device group ID (--deqp-vk-device-group-id)
    int getVKDeviceGroupId(void) const;

    //! Enable development-time test case validation checks
    bool isValidationEnabled(void) const;

    //! Enable SPIRV validation checks (--deqp-spirv-validation)
    bool isSpirvValidationEnabled(void) const;

    //! Print validation errors to standard error or keep them in the log only.
    bool printValidationErrors(void) const;

    //! Enable runtime check for duplicate case names in test hierarchy
    bool checkDuplicateCaseNames(void) const;

    //! Log of decompiled SPIR-V shader source (--deqp-log-decompiled-spirv)
    bool isLogDecompiledSpirvEnabled(void) const;

    //! Should we run tests that exhaust memory (--deqp-test-oom)
    bool isOutOfMemoryTestEnabled(void) const;

    //! Should the shader cache be enabled (--deqp-shadercache)
    bool isShadercacheEnabled(void) const;

    //! Get the filename for shader cache (--deqp-shadercache-filename)
    const char *getShaderCacheFilename(void) const;

    //! Should the shader cache be truncated before run (--deqp-shadercache-truncate)
    bool isShaderCacheTruncateEnabled(void) const;

    //! Should the shader cache use inter process communication (IPC) (--deqp-shadercache-ipc)
    bool isShaderCacheIPCEnabled(void) const;

    //! Get shader optimization recipe (--deqp-optimization-recipe)
    int getOptimizationRecipe(void) const;

    //! Enable optimizing of spir-v (--deqp-optimize-spirv)
    bool isSpirvOptimizationEnabled(void) const;

    //! Enable RenderDoc frame markers (--deqp-renderdoc)
    bool isRenderDocEnabled(void) const;

    //! Get waiver file name (--deqp-waiver-file)
    const char *getWaiverFileName(void) const;

    //! Get case list fraction
    const std::vector<int> &getCaseFraction(void) const;

    //! Get must-list filename
    const char *getCaseFractionMandatoryTests(void) const;

    //! Get archive directory path
    const char *getArchiveDir(void) const;

    //! Get runner type (--deqp-runner-type)
    tcu::TestRunnerType getRunnerType(void) const;

    //! Should the run be terminated on first failure (--deqp-terminate-on-fail)
    bool isTerminateOnFailEnabled(void) const;

    //! Should the run be terminated on first device lost (--deqp-terminate-on-device-lost)
    bool isTerminateOnDeviceLostEnabled(void) const;

    //! Start as subprocess ( Vulkan SC )
    bool isSubProcess(void) const;

    //! Define default number of tests performed in main process ( Vulkan SC )
    int getSubprocessTestCount(void) const;

    //! Config file defining number of tests performed in subprocess for specific test branches
    const char *getSubprocessConfigFile(void) const;

    //! Optional server address that will be responsible for (among other things) compiling shaders ( Vulkan SC )
    const char *getServerAddress(void) const;

    //! Define minimum size of a single command pool ( Vulkan SC )
    int getCommandPoolMinSize(void) const;

    //! Define minimum size of a single command buffer ( Vulkan SC )
    int getCommandBufferMinSize(void) const;

    //! Define default size for single command in command buffer ( Vulkan SC )
    int getCommandDefaultSize(void) const;

    //! Define default size for single pipeline ( Vulkan SC )
    int getPipelineDefaultSize(void) const;

    //! Path to offline pipeline compiler executable
    const char *getPipelineCompilerPath(void) const;

    //! Directory containing input and output pipeline compiler files
    const char *getPipelineCompilerDataDir(void) const;

    //! Additional args for offline pipeline compiler
    const char *getPipelineCompilerArgs(void) const;

    //! Output pipeline cache file produced by offline pipeline compiler
    const char *getPipelineCompilerOutputFile(void) const;

    //! Log file for offline pipeline compiler
    const char *getPipelineCompilerLogFile(void) const;

    //! Prefix for offline pipeline compiler input files
    const char *getPipelineCompilerFilePrefix(void) const;

    // Print logs of video operations to stdout
    bool getVideoLogPrint(void) const;

    // Dump output of vulkan video decoding tests.
    VideoDecodeOutput getVideoDumpDecodeOutput(void) const;

    // Dump output of vulkan video encoding tests.
    VideoEncodeOutput getVideoDumpEncodeOutput(void) const;

    //! Path to Vulkan library (e.g. loader library vulkan-1.dll)
    const char *getVkLibraryPath(void) const;

    //! File that provides a default set of application parameters
    const char *getAppParamsInputFilePath(void) const;

    //! Perform tests for devices implementing compute-only functionality
    bool isComputeOnly(void) const;

    //! Allows you to use vendor-specific configuration
    bool isVendorSpecific() const;

    /*--------------------------------------------------------------------*//*!
     * \brief Creates case list filter
     * \param archive Resources
     *
     * Creates case list filter based on one of the following parameters:
     *
     * --deqp-case
     * --deqp-caselist
     * --deqp-caselist-file
     * --deqp-caselist-resource
     * --deqp-stdin-caselist
     *
     * Throws std::invalid_argument if parsing fails.
     *//*--------------------------------------------------------------------*/
    de::MovePtr<CaseListFilter> createCaseListFilter(const tcu::Archive &archive) const;

protected:
    const de::cmdline::CommandLine &getCommandLine(void) const;

private:
    CommandLine(const CommandLine &);            // not allowed!
    CommandLine &operator=(const CommandLine &); // not allowed!

    void clear(void);

    virtual void registerExtendedOptions(de::cmdline::Parser &parser);

    std::string m_appName;
    de::cmdline::CommandLine m_cmdLine;
    uint32_t m_logFlags;

    std::string m_initialCmdLine;
    bool m_hadHelpSpecified;
};

} // namespace tcu

#endif // _TCUCOMMANDLINE_HPP
