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

#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "tcuTestCase.hpp"
#include "tcuResource.hpp"
#include "deFilePath.hpp"
#include "deStringUtil.hpp"
#include "deString.h"
#include "deInt32.h"
#include "deCommandLine.h"
#include "qpTestLog.h"
#include "qpDebugOut.h"

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>

using std::string;
using std::vector;

// OOM tests are enabled by default only on platforms that don't do memory overcommit (Win32)
#if (DE_OS == DE_OS_WIN32)
#define TEST_OOM_DEFAULT "enable"
#else
#define TEST_OOM_DEFAULT "disable"
#endif

// Duplicate name checks are enabled by default in Debug mode, and disabled in Release mode.
#if defined(DE_DEBUG)
#define DUPLICATE_CHECK_DEFAULT "enable"
#else
#define DUPLICATE_CHECK_DEFAULT "disable"
#endif

// SPIRV validation is enabled by default in Debug mode, and disabled in Release mode.
#if defined(DE_DEBUG)
#define SPIRV_VALIDATION_DEFAULT "enable"
#else
#define SPIRV_VALIDATION_DEFAULT "disable"
#endif

namespace tcu
{

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(CasePath, std::string);
DE_DECLARE_COMMAND_LINE_OPT(ExcludeCasePath, std::string);
DE_DECLARE_COMMAND_LINE_OPT(CaseList, std::string);
DE_DECLARE_COMMAND_LINE_OPT(CaseListFile, std::string);
DE_DECLARE_COMMAND_LINE_OPT(CaseListResource, std::string);
DE_DECLARE_COMMAND_LINE_OPT(StdinCaseList, bool);
DE_DECLARE_COMMAND_LINE_OPT(LogFilename, std::string);
DE_DECLARE_COMMAND_LINE_OPT(RunMode, tcu::RunMode);
DE_DECLARE_COMMAND_LINE_OPT(ExportFilenamePattern, std::string);
DE_DECLARE_COMMAND_LINE_OPT(MustpassSpec, std::string);
DE_DECLARE_COMMAND_LINE_OPT(WatchDog, bool);
DE_DECLARE_COMMAND_LINE_OPT(CrashHandler, bool);
DE_DECLARE_COMMAND_LINE_OPT(BaseSeed, int);
DE_DECLARE_COMMAND_LINE_OPT(TestIterationCount, int);
DE_DECLARE_COMMAND_LINE_OPT(Visibility, WindowVisibility);
DE_DECLARE_COMMAND_LINE_OPT(SurfaceWidth, int);
DE_DECLARE_COMMAND_LINE_OPT(SurfaceHeight, int);
DE_DECLARE_COMMAND_LINE_OPT(SurfaceType, tcu::SurfaceType);
DE_DECLARE_COMMAND_LINE_OPT(ScreenRotation, tcu::ScreenRotation);
DE_DECLARE_COMMAND_LINE_OPT(GLContextType, std::string);
DE_DECLARE_COMMAND_LINE_OPT(GLConfigID, int);
DE_DECLARE_COMMAND_LINE_OPT(GLConfigName, std::string);
DE_DECLARE_COMMAND_LINE_OPT(GLContextFlags, std::string);
DE_DECLARE_COMMAND_LINE_OPT(CLPlatformID, int);
DE_DECLARE_COMMAND_LINE_OPT(CLDeviceIDs, std::vector<int>);
DE_DECLARE_COMMAND_LINE_OPT(CLBuildOptions, std::string);
DE_DECLARE_COMMAND_LINE_OPT(EGLDisplayType, std::string);
DE_DECLARE_COMMAND_LINE_OPT(EGLWindowType, std::string);
DE_DECLARE_COMMAND_LINE_OPT(EGLPixmapType, std::string);
DE_DECLARE_COMMAND_LINE_OPT(LogImages, bool);
DE_DECLARE_COMMAND_LINE_OPT(LogAllImages, bool);
DE_DECLARE_COMMAND_LINE_OPT(LogShaderSources, bool);
DE_DECLARE_COMMAND_LINE_OPT(LogDecompiledSpirv, bool);
DE_DECLARE_COMMAND_LINE_OPT(LogEmptyLoginfo, bool);
DE_DECLARE_COMMAND_LINE_OPT(TestOOM, bool);
DE_DECLARE_COMMAND_LINE_OPT(ArchiveDir, std::string);
DE_DECLARE_COMMAND_LINE_OPT(VKDeviceID, int);
DE_DECLARE_COMMAND_LINE_OPT(MaxCustomDevices, int);
DE_DECLARE_COMMAND_LINE_OPT(VKDeviceGroupID, int);
DE_DECLARE_COMMAND_LINE_OPT(LogFlush, bool);
DE_DECLARE_COMMAND_LINE_OPT(LogCompact, bool);
DE_DECLARE_COMMAND_LINE_OPT(Validation, bool);
DE_DECLARE_COMMAND_LINE_OPT(SpirvValidation, bool);
DE_DECLARE_COMMAND_LINE_OPT(PrintValidationErrors, bool);
DE_DECLARE_COMMAND_LINE_OPT(DuplicateCheck, bool);
DE_DECLARE_COMMAND_LINE_OPT(ShaderCache, bool);
DE_DECLARE_COMMAND_LINE_OPT(ShaderCacheFilename, std::string);
DE_DECLARE_COMMAND_LINE_OPT(Optimization, int);
DE_DECLARE_COMMAND_LINE_OPT(OptimizeSpirv, bool);
DE_DECLARE_COMMAND_LINE_OPT(ShaderCacheTruncate, bool);
DE_DECLARE_COMMAND_LINE_OPT(ShaderCacheIPC, bool);
DE_DECLARE_COMMAND_LINE_OPT(RenderDoc, bool);
DE_DECLARE_COMMAND_LINE_OPT(CaseFraction, std::vector<int>);
DE_DECLARE_COMMAND_LINE_OPT(CaseFractionMandatoryTests, std::string);
DE_DECLARE_COMMAND_LINE_OPT(WaiverFile, std::string);
DE_DECLARE_COMMAND_LINE_OPT(RunnerType, tcu::TestRunnerType);
DE_DECLARE_COMMAND_LINE_OPT(TerminateOnFail, bool);
DE_DECLARE_COMMAND_LINE_OPT(TerminateOnDeviceLost, bool);
DE_DECLARE_COMMAND_LINE_OPT(SubProcess, bool);
DE_DECLARE_COMMAND_LINE_OPT(SubprocessTestCount, int);
DE_DECLARE_COMMAND_LINE_OPT(SubprocessConfigFile, std::string);
DE_DECLARE_COMMAND_LINE_OPT(ServerAddress, std::string);
DE_DECLARE_COMMAND_LINE_OPT(CommandPoolMinSize, int);
DE_DECLARE_COMMAND_LINE_OPT(CommandBufferMinSize, int);
DE_DECLARE_COMMAND_LINE_OPT(CommandDefaultSize, int);
DE_DECLARE_COMMAND_LINE_OPT(PipelineDefaultSize, int);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerPath, std::string);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerDataDir, std::string);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerArgs, std::string);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerOutputFile, std::string);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerLogFile, std::string);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerFilePrefix, std::string);
DE_DECLARE_COMMAND_LINE_OPT(VkLibraryPath, std::string);
DE_DECLARE_COMMAND_LINE_OPT(ApplicationParametersInputFile, std::string);
DE_DECLARE_COMMAND_LINE_OPT(QuietStdout, bool);
DE_DECLARE_COMMAND_LINE_OPT(ComputeOnly, bool);
DE_DECLARE_COMMAND_LINE_OPT(VideoLogPrint, bool);
DE_DECLARE_COMMAND_LINE_OPT(VideoDecodeOutputDump, VideoDecodeOutput);
DE_DECLARE_COMMAND_LINE_OPT(VideoEncodeOutputDump, VideoEncodeOutput);
DE_DECLARE_COMMAND_LINE_OPT(VendorSpecific, bool);

static void parseIntList(const char *src, std::vector<int> *dst)
{
    std::istringstream str(src);
    std::string val;

    while (std::getline(str, val, ','))
    {
        int intVal = 0;
        de::cmdline::parseType(val.c_str(), &intVal);
        dst->push_back(intVal);
    }
}

void registerOptions(de::cmdline::Parser &parser)
{
    using de::cmdline::NamedValue;
    using de::cmdline::Option;

    static const NamedValue<bool> s_enableNames[]                    = {{"enable", true}, {"disable", false}};
    static const NamedValue<tcu::RunMode> s_runModes[]               = {{"execute", RUNMODE_EXECUTE},
                                                                        {"xml-caselist", RUNMODE_DUMP_XML_CASELIST},
                                                                        {"txt-caselist", RUNMODE_DUMP_TEXT_CASELIST},
                                                                        {"stdout-caselist", RUNMODE_DUMP_STDOUT_CASELIST},
                                                                        {"amber-verify", RUNMODE_VERIFY_AMBER_COHERENCY},
                                                                        {"txt-trie", RUNMODE_DUMP_TEXT_TRIE},
                                                                        {"stdout-trie", RUNMODE_DUMP_STDOUT_TRIE},
                                                                        {"gen-mustpass", RUNMODE_GEN_MUSTPASS}};
    static const NamedValue<WindowVisibility> s_visibilites[]        = {{"windowed", WINDOWVISIBILITY_WINDOWED},
                                                                        {"fullscreen", WINDOWVISIBILITY_FULLSCREEN},
                                                                        {"hidden", WINDOWVISIBILITY_HIDDEN}};
    static const NamedValue<tcu::SurfaceType> s_surfaceTypes[]       = {{"window", SURFACETYPE_WINDOW},
                                                                        {"pixmap", SURFACETYPE_OFFSCREEN_NATIVE},
                                                                        {"pbuffer", SURFACETYPE_OFFSCREEN_GENERIC},
                                                                        {"fbo", SURFACETYPE_FBO}};
    static const NamedValue<tcu::ScreenRotation> s_screenRotations[] = {{"unspecified", SCREENROTATION_UNSPECIFIED},
                                                                        {"0", SCREENROTATION_0},
                                                                        {"90", SCREENROTATION_90},
                                                                        {"180", SCREENROTATION_180},
                                                                        {"270", SCREENROTATION_270}};
    static const NamedValue<tcu::TestRunnerType> s_runnerTypes[]     = {
        {"any", tcu::RUNNERTYPE_ANY},
        {"none", tcu::RUNNERTYPE_NONE},
        {"amber", tcu::RUNNERTYPE_AMBER},
    };

    static const NamedValue<VideoDecodeOutput> s_videoDecodeDump[] = {
        {"disable", DUMP_DEC_DISABLE},
        {"single", DUMP_DEC_TO_SINGLE},
        {"separate", DUMP_DEC_TO_SEPARATE_FILES},
    };

    static const NamedValue<VideoEncodeOutput> s_videoEncodeDump[] = {
        {"disable", DUMP_ENC_DISABLE},
        {"yuv", DUMP_ENC_YUV},
        {"bitstream", DUMP_ENC_BITSTREAM},
        {"all", DUMP_ENC_ALL},
    };

    parser
        << Option<QuietStdout>("q", "quiet", "Suppress messages to standard output")
        << Option<CasePath>("n", "deqp-case",
                            "Test case(s) to run, supports wildcards (e.g. dEQP-GLES2.info.*) and commas to separate "
                            "multiple patterns")
        << Option<ExcludeCasePath>("e", "deqp-exclude-case",
                                   "Test case(s) to exclude, supports wildcards (e.g. dEQP-GLES2.info.*) and commas to "
                                   "separate multiple patterns")
        << Option<CaseListFile>("f", "deqp-caselist-file", "Read case list (in trie format) from given file")
        << Option<CaseList>(nullptr, "deqp-caselist",
                            "Case list to run in trie format (e.g. {dEQP-GLES2{info{version,renderer}}})")
        << Option<CaseListResource>(nullptr, "deqp-caselist-resource",
                                    "Read case list (in trie format) from given file located application's assets")
        << Option<StdinCaseList>(nullptr, "deqp-stdin-caselist", "Read case list (in trie format) from stdin")
        << Option<LogFilename>(nullptr, "deqp-log-filename", "Write test results to given file", "TestResults.qpa")
        << Option<RunMode>(nullptr, "deqp-runmode",
                           "Execute tests, write list of test cases into a file, or verify amber capability coherency",
                           s_runModes, "execute")
        << Option<ExportFilenamePattern>(nullptr, "deqp-caselist-export-file",
                                         "Set the target file name pattern for caselist export",
                                         "${packageName}-cases.${typeExtension}")
        << Option<MustpassSpec>(nullptr, "deqp-mustpass-spec",
                                "Path to a mustpass spec file describing per-configuration filters and outputs "
                                "(used with --deqp-runmode=gen-mustpass)",
                                "")
        << Option<WatchDog>(nullptr, "deqp-watchdog", "Enable test watchdog", s_enableNames, "disable")
        << Option<CrashHandler>(nullptr, "deqp-crashhandler", "Enable crash handling", s_enableNames, "disable")
        << Option<BaseSeed>(nullptr, "deqp-base-seed", "Base seed for test cases that use randomization", "0")
        << Option<TestIterationCount>(nullptr, "deqp-test-iteration-count",
                                      "Iteration count for cases that support variable number of iterations", "0")
        << Option<Visibility>(nullptr, "deqp-visibility", "Default test window visibility", s_visibilites, "windowed")
        << Option<SurfaceWidth>(nullptr, "deqp-surface-width", "Use given surface width if possible", "-1")
        << Option<SurfaceHeight>(nullptr, "deqp-surface-height", "Use given surface height if possible", "-1")
        << Option<SurfaceType>(nullptr, "deqp-surface-type", "Use given surface type", s_surfaceTypes, "window")
        << Option<ScreenRotation>(nullptr, "deqp-screen-rotation", "Screen rotation for platforms that support it",
                                  s_screenRotations, "0")
        << Option<GLContextType>(nullptr, "deqp-gl-context-type",
                                 "OpenGL context type for platforms that support multiple")
        << Option<GLConfigID>(nullptr, "deqp-gl-config-id",
                              "OpenGL (ES) render config ID (EGL config id on EGL platforms)", "-1")
        << Option<GLConfigName>(nullptr, "deqp-gl-config-name", "Symbolic OpenGL (ES) render config name")
        << Option<GLContextFlags>(nullptr, "deqp-gl-context-flags",
                                  "OpenGL context flags (comma-separated, supports debug and robust)")
        << Option<CLPlatformID>(nullptr, "deqp-cl-platform-id",
                                "Execute tests on given OpenCL platform (IDs start from 1)", "1")
        << Option<CLDeviceIDs>(nullptr, "deqp-cl-device-ids",
                               "Execute tests on given CL devices (comma-separated, IDs start from 1)", parseIntList,
                               "")
        << Option<CLBuildOptions>(nullptr, "deqp-cl-build-options", "Extra build options for OpenCL compiler")
        << Option<EGLDisplayType>(nullptr, "deqp-egl-display-type", "EGL native display type")
        << Option<EGLWindowType>(nullptr, "deqp-egl-window-type", "EGL native window type")
        << Option<EGLPixmapType>(nullptr, "deqp-egl-pixmap-type", "EGL native pixmap type")
        << Option<VKDeviceID>(nullptr, "deqp-vk-device-id", "Vulkan device ID (IDs start from 1)", "1")
        << Option<MaxCustomDevices>(nullptr, "deqp-max-custom-vk-devices", "Maximum number of custom devices", "5")
        << Option<VKDeviceGroupID>(nullptr, "deqp-vk-device-group-id", "Vulkan device Group ID (IDs start from 1)", "1")
        << Option<LogImages>(nullptr, "deqp-log-images",
                             "When disabled, prevent any image from being logged into the test results file",
                             s_enableNames, "enable")
        << Option<LogAllImages>(nullptr, "deqp-log-all-images",
                                "When enabled, log all images from image comparison routines as if "
                                "COMPARE_LOG_EVERYTHING was used in the code",
                                s_enableNames, "disable")
        << Option<LogShaderSources>(nullptr, "deqp-log-shader-sources", "Enable or disable logging of shader sources",
                                    s_enableNames, "enable")
        << Option<LogDecompiledSpirv>(nullptr, "deqp-log-decompiled-spirv",
                                      "Enable or disable logging of decompiled spir-v", s_enableNames, "enable")
        << Option<LogEmptyLoginfo>(nullptr, "deqp-log-empty-loginfo", "Logging of empty shader compile/link log info",
                                   s_enableNames, "enable")
        << Option<TestOOM>(nullptr, "deqp-test-oom", "Run tests that exhaust memory on purpose", s_enableNames,
                           TEST_OOM_DEFAULT)
        << Option<ArchiveDir>(nullptr, "deqp-archive-dir", "Path to test resource files", ".")
        << Option<LogFlush>(nullptr, "deqp-log-flush", "Enable or disable log file fflush", s_enableNames, "enable")
        << Option<LogCompact>(nullptr, "deqp-log-compact", "Enable or disable the compact version of the log",
                              s_enableNames, "disable")
        << Option<Validation>(nullptr, "deqp-vk-validation", "Enable or disable test case validation", s_enableNames,
                              "disable")
        << Option<SpirvValidation>(nullptr, "deqp-spirv-validation", "Enable or disable spir-v shader validation",
                                   s_enableNames, SPIRV_VALIDATION_DEFAULT)
        << Option<PrintValidationErrors>(nullptr, "deqp-vk-print-validation-errors",
                                         "Print validation errors to standard error")
        << Option<DuplicateCheck>(nullptr, "deqp-duplicate-case-name-check",
                                  "Check for duplicate case names when creating test hierarchy", s_enableNames,
                                  DUPLICATE_CHECK_DEFAULT)
        << Option<Optimization>(nullptr, "deqp-optimization-recipe",
                                "Shader optimization recipe (0=disabled, 1=performance, 2=size)", "0")
        << Option<OptimizeSpirv>(nullptr, "deqp-optimize-spirv", "Apply optimization to spir-v shaders as well",
                                 s_enableNames, "disable")
        << Option<ShaderCache>(nullptr, "deqp-shadercache", "Enable or disable shader cache", s_enableNames, "enable")
        << Option<ShaderCacheFilename>(nullptr, "deqp-shadercache-filename", "Write shader cache to given file",
                                       "shadercache.bin")
        << Option<ShaderCacheTruncate>(nullptr, "deqp-shadercache-truncate",
                                       "Truncate shader cache before running tests", s_enableNames, "enable")
        << Option<ShaderCacheIPC>(nullptr, "deqp-shadercache-ipc", "Should shader cache use inter process comms",
                                  s_enableNames, "disable")
        << Option<RenderDoc>(nullptr, "deqp-renderdoc", "Enable RenderDoc frame markers", s_enableNames, "disable")
        << Option<CaseFraction>(nullptr, "deqp-fraction",
                                "Run a fraction of the test cases (e.g. N,M means run group%M==N)", parseIntList, "")
        << Option<CaseFractionMandatoryTests>(nullptr, "deqp-fraction-mandatory-caselist-file",
                                              "Case list file that must be run for each fraction", "")
        << Option<WaiverFile>(nullptr, "deqp-waiver-file", "Read waived tests from given file", "")
        << Option<RunnerType>(nullptr, "deqp-runner-type", "Filter test cases based on runner", s_runnerTypes, "any")
        << Option<TerminateOnFail>(nullptr, "deqp-terminate-on-fail", "Terminate the run on first failure",
                                   s_enableNames, "disable")
        << Option<TerminateOnDeviceLost>(nullptr, "deqp-terminate-on-device-lost",
                                         "Terminate the run on a device lost error", s_enableNames, "enable")
        << Option<SubProcess>(nullptr, "deqp-subprocess",
                              "Inform app that it works as subprocess (Vulkan SC only, do not use manually)",
                              s_enableNames, "disable")
        << Option<SubprocessTestCount>(
               nullptr, "deqp-subprocess-test-count",
               "Define default number of tests performed in subprocess for specific test cases(Vulkan SC only)",
               "65536")
        << Option<SubprocessConfigFile>(nullptr, "deqp-subprocess-cfg-file",
                                        "Config file defining number of tests performed in subprocess for specific "
                                        "test branches (Vulkan SC only)",
                                        "")
        << Option<ServerAddress>(nullptr, "deqp-server-address",
                                 "Server address (host:port) responsible for shader compilation (Vulkan SC only)", "")
        << Option<CommandPoolMinSize>(nullptr, "deqp-command-pool-min-size",
                                      "Define minimum size of the command pool (in bytes) to use (Vulkan SC only)", "0")
        << Option<CommandBufferMinSize>(nullptr, "deqp-command-buffer-min-size",
                                        "Define minimum size of the command buffer (in bytes) to use (Vulkan SC only)",
                                        "0")
        << Option<CommandDefaultSize>(nullptr, "deqp-command-default-size",
                                      "Define default single command size (in bytes) to use (Vulkan SC only)", "256")
        << Option<PipelineDefaultSize>(nullptr, "deqp-pipeline-default-size",
                                       "Define default pipeline size (in bytes) to use (Vulkan SC only)", "16384")
        << Option<PipelineCompilerPath>(nullptr, "deqp-pipeline-compiler",
                                        "Path to offline pipeline compiler (Vulkan SC only)", "")
        << Option<PipelineCompilerDataDir>(nullptr, "deqp-pipeline-dir",
                                           "Offline pipeline data directory (Vulkan SC only)", "")
        << Option<PipelineCompilerArgs>(nullptr, "deqp-pipeline-args",
                                        "Additional compiler parameters (Vulkan SC only)", "")
        << Option<PipelineCompilerOutputFile>(nullptr, "deqp-pipeline-file",
                                              "Output file with pipeline cache (Vulkan SC only, do not use manually)",
                                              "")
        << Option<PipelineCompilerLogFile>(nullptr, "deqp-pipeline-logfile",
                                           "Log file for pipeline compiler (Vulkan SC only, do not use manually)", "")
        << Option<PipelineCompilerFilePrefix>(
               nullptr, "deqp-pipeline-prefix",
               "Prefix for input pipeline compiler files (Vulkan SC only, do not use manually)", "")
        << Option<VkLibraryPath>(nullptr, "deqp-vk-library-path",
                                 "Path to Vulkan library (e.g. loader library vulkan-1.dll)", "")
        << Option<ApplicationParametersInputFile>(nullptr, "deqp-app-params-input-file",
                                                  "File that provides a default set of application parameters")
        << Option<ComputeOnly>(nullptr, "deqp-compute-only",
                               "Perform tests for devices implementing compute-only functionality", s_enableNames,
                               "disable")
        << Option<VideoLogPrint>(nullptr, "deqp-vk-video-log-print", "Print log messages of vulkan video tests",
                                 s_enableNames, "disable")
        << Option<VideoDecodeOutputDump>(nullptr, "deqp-vk-video-decode-dump",
                                         "Dump the output of vulkan video decoding tests", s_videoDecodeDump, "disable")
        << Option<VideoEncodeOutputDump>(nullptr, "deqp-vk-video-encode-dump",
                                         "Dump the output of vulkan video encoding tests", s_videoEncodeDump, "disable")
        << Option<VendorSpecific>(nullptr, "deqp-vk-vendor-specific", "Allows you to use vendor-specific configuration",
                                  s_enableNames, "disable");
}

void registerLegacyOptions(de::cmdline::Parser &parser)
{
    using de::cmdline::Option;

    parser << Option<GLConfigID>(nullptr, "deqp-egl-config-id", "Legacy name for --deqp-gl-config-id", "-1")
           << Option<GLConfigName>(nullptr, "deqp-egl-config-name", "Legacy name for --deqp-gl-config-name");
}

} // namespace opt

// Used to store hashes of test case names
typedef uint64_t test_case_hash_t;

// Source: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
// MurmurHash2, 64-bit versions, by Austin Appleby
static uint64_t MurmurHash64B(const void *key, int len, uint64_t seed)
{
    const uint32_t m = 0x5bd1e995;
    const int r      = 24;

    uint32_t h1 = uint32_t(seed) ^ len;
    uint32_t h2 = uint32_t(seed >> 32);

    // Ensure that unaligned accesses to data are allowed.
#ifdef WIN32
    typedef __declspec(align(1)) uint32_t uint32_t_unaligned;
#else
    typedef __attribute__((aligned(1))) uint32_t uint32_t_unaligned;
#endif
    const uint32_t_unaligned *data = (const uint32_t_unaligned *)key;

    while (len >= 8)
    {
        uint32_t k1 = *data++;
        k1 *= m;
        k1 ^= k1 >> r;
        k1 *= m;
        h1 *= m;
        h1 ^= k1;
        len -= 4;

        uint32_t k2 = *data++;
        k2 *= m;
        k2 ^= k2 >> r;
        k2 *= m;
        h2 *= m;
        h2 ^= k2;
        len -= 4;
    }

    if (len >= 4)
    {
        uint32_t k1 = *data++;
        k1 *= m;
        k1 ^= k1 >> r;
        k1 *= m;
        h1 *= m;
        h1 ^= k1;
        len -= 4;
    }

    switch (len)
    {
    case 3:
        h2 ^= ((unsigned char *)data)[2] << 16;
        // fall through
    case 2:
        h2 ^= ((unsigned char *)data)[1] << 8;
        // fall through
    case 1:
        h2 ^= ((unsigned char *)data)[0];
        h2 *= m;
    };

    h1 ^= h2 >> 18;
    h1 *= m;
    h2 ^= h1 >> 22;
    h2 *= m;
    h1 ^= h2 >> 17;
    h1 *= m;
    h2 ^= h1 >> 19;
    h2 *= m;

    uint64_t h = h1;

    h = (h << 32) | h2;

    return h;
}

/*--------------------------------------------------------------------*//*!
 * \brief Generates an hash for the test case name part provided.
 * If a hashCollisionDetectionMap is passed, will detect hash
 * collisions using that map. hashCollisionDetectionMap can be NULL.
 * As an example, the standard std::hash<std::string> on a 32-bit
 * machine will collide with 'random_298' and 'subgroupand_int16_t_mesh_requiredsubgroupsize'
 *//*--------------------------------------------------------------------*/
static test_case_hash_t hashTestNodeName(const std::string &name,
                                         std::unordered_map<test_case_hash_t, std::string> *hashCollisionDetectionMap)
{
    test_case_hash_t hash = MurmurHash64B(name.c_str(), (int)name.length(), 1);
    if (hashCollisionDetectionMap != nullptr)
    {
        auto search = hashCollisionDetectionMap->find(hash);
        if (search != hashCollisionDetectionMap->end())
        {
            if (search->second != name)
            {
                print("There was an hash collision between '%s' and '%s'\n", search->second.c_str(), name.c_str());
                throw std::runtime_error("Hash collision detected!");
            }
        }
        hashCollisionDetectionMap->insert({hash, name});
    }
    return hash;
}

// \todo [2014-02-13 pyry] This could be useful elsewhere as well.
class DebugOutStreambuf : public std::streambuf
{
public:
    DebugOutStreambuf(void);
    ~DebugOutStreambuf(void);

protected:
    std::streamsize xsputn(const char *s, std::streamsize count);
    int overflow(int ch = -1);

private:
    void flushLine(void);

    std::ostringstream m_curLine;
};

DebugOutStreambuf::DebugOutStreambuf(void)
{
}

DebugOutStreambuf::~DebugOutStreambuf(void)
{
    if (m_curLine.tellp() != std::streampos(0))
        flushLine();
}

std::streamsize DebugOutStreambuf::xsputn(const char *s, std::streamsize count)
{
    for (std::streamsize pos = 0; pos < count; pos++)
    {
        m_curLine.put(s[pos]);

        if (s[pos] == '\n')
            flushLine();
    }

    return count;
}

int DebugOutStreambuf::overflow(int ch)
{
    if (ch == -1)
        return -1;
    else
    {
        DE_ASSERT((ch & 0xff) == ch);
        const char chVal = (char)(uint8_t)(ch & 0xff);
        return xsputn(&chVal, 1) == 1 ? ch : -1;
    }
}

void DebugOutStreambuf::flushLine(void)
{
    qpPrint(m_curLine.str().c_str());
    m_curLine.str("");
}

class CaseTreeNode
{
public:
    CaseTreeNode(const test_case_hash_t hash) : m_hash(hash)
    {
    }
    ~CaseTreeNode(void);

    test_case_hash_t getHash(void) const
    {
        return m_hash;
    }
    bool hasChildren(void) const
    {
        return !m_children.empty();
    }

    bool hasChild(test_case_hash_t hash) const;
    CaseTreeNode *getChild(test_case_hash_t hash) const;

    void addChild(CaseTreeNode *child)
    {
        m_children.push_back(child);
    }

private:
    CaseTreeNode(const CaseTreeNode &);
    CaseTreeNode &operator=(const CaseTreeNode &);

    enum
    {
        NOT_FOUND = -1
    };

    int findChildNdx(test_case_hash_t hash) const;

    test_case_hash_t m_hash;
    std::vector<CaseTreeNode *> m_children;
};

CaseTreeNode::~CaseTreeNode(void)
{
    for (vector<CaseTreeNode *>::const_iterator i = m_children.begin(); i != m_children.end(); ++i)
        delete *i;
}

int CaseTreeNode::findChildNdx(test_case_hash_t hash) const
{
    for (int ndx = 0; ndx < (int)m_children.size(); ++ndx)
    {
        if (m_children[ndx]->getHash() == hash)
            return ndx;
    }
    return NOT_FOUND;
}

inline bool CaseTreeNode::hasChild(test_case_hash_t hash) const
{
    return findChildNdx(hash) != NOT_FOUND;
}

inline CaseTreeNode *CaseTreeNode::getChild(test_case_hash_t hash) const
{
    const int ndx = findChildNdx(hash);
    return ndx == NOT_FOUND ? nullptr : m_children[ndx];
}

static int getCurrentComponentLen(const char *path)
{
    int ndx = 0;
    for (; path[ndx] != 0 && path[ndx] != '.'; ++ndx)
        ;
    return ndx;
}

static const CaseTreeNode *findNode(const CaseTreeNode *root, const char *path)
{
    const CaseTreeNode *curNode = root;
    const char *curPath         = path;
    int curLen                  = getCurrentComponentLen(curPath);

    for (;;)
    {
        test_case_hash_t hash = hashTestNodeName(std::string(curPath, curPath + curLen), nullptr);
        curNode               = curNode->getChild(hash);

        if (!curNode)
            break;

        curPath += curLen;

        if (curPath[0] == 0)
            break;
        else
        {
            DE_ASSERT(curPath[0] == '.');
            curPath += 1;
            curLen = getCurrentComponentLen(curPath);
        }
    }

    return curNode;
}

static void parseCaseTrie(CaseTreeNode *root, std::istream &in,
                          std::unordered_map<test_case_hash_t, string> &hashCollisionDetectionMap)
{
    vector<CaseTreeNode *> nodeStack;
    string curName;
    bool expectNode = true;

    if (in.get() != '{')
        throw std::invalid_argument("Malformed case trie");

    nodeStack.push_back(root);

    while (!nodeStack.empty())
    {
        const int curChr = in.get();

        if (curChr == std::char_traits<char>::eof() || curChr == 0)
            throw std::invalid_argument("Unterminated case tree");

        if (curChr == '{' || curChr == ',' || curChr == '}')
        {
            if (!curName.empty() && expectNode)
            {
                test_case_hash_t hash        = hashTestNodeName(curName, &hashCollisionDetectionMap);
                CaseTreeNode *const newChild = new CaseTreeNode(hash);

                try
                {
                    nodeStack.back()->addChild(newChild);
                }
                catch (...)
                {
                    delete newChild;
                    throw;
                }

                if (curChr == '{')
                    nodeStack.push_back(newChild);

                curName.clear();
            }
            else if (curName.empty() == expectNode)
                throw std::invalid_argument(expectNode ? "Empty node name" : "Missing node separator");

            if (curChr == '}')
            {
                expectNode = false;
                nodeStack.pop_back();

                // consume trailing new line
                if (nodeStack.empty())
                {
                    if (in.peek() == '\r')
                        in.get();
                    if (in.peek() == '\n')
                        in.get();
                }
            }
            else
                expectNode = true;
        }
        else if (isValidTestCaseNameChar((char)curChr))
            curName += (char)curChr;
        else
            throw std::invalid_argument("Illegal character in node name");
    }
}

static void trimString(std::string &s)
{
    // Trim at the start.
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));

    // Trim at the end.
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

static void parseSimpleCaseList(vector<CaseTreeNode *> &nodeStack, std::istream &in, bool reportDuplicates,
                                std::unordered_map<test_case_hash_t, string> &hashCollisionDetectionMap)
{
    // \note Algorithm assumes that cases are sorted by groups, but will
    //         function fine, albeit more slowly, if that is not the case.
    int stackPos = 0;
    string curName;
    string line;

    // Outer loop to iterate every line.
    while (in.good())
    {
        std::getline(in, line);
        if (in.fail())
            break;

        trimString(line);
        if (line.empty() || line.front() == '#') // Ignore empty lines and comments.
            continue;

        std::istringstream inputLine(line);

        for (;;)
        {
            const int curChr = inputLine.get();

            if (curChr == std::char_traits<char>::eof() || curChr == 0 || curChr == '\n' || curChr == '\r')
            {
                if (curName.empty())
                    throw std::invalid_argument("Empty test case name");

                test_case_hash_t hash = hashTestNodeName(curName, &hashCollisionDetectionMap);
                if (!nodeStack[stackPos]->hasChild(hash))
                {
                    CaseTreeNode *const newChild = new CaseTreeNode(hash);

                    try
                    {
                        nodeStack[stackPos]->addChild(newChild);
                    }
                    catch (...)
                    {
                        delete newChild;
                        throw;
                    }
                }
                else if (reportDuplicates)
                    throw std::invalid_argument("Duplicate test case");

                curName.clear();
                stackPos = 0;

                if (curChr == '\r' && inputLine.peek() == '\n')
                    inputLine.get();

                {
                    const int nextChr = inputLine.peek();

                    if (nextChr == std::char_traits<char>::eof() || nextChr == 0)
                        break;
                }
            }
            else if (curChr == '.')
            {
                if (curName.empty())
                    throw std::invalid_argument("Empty test group name");

                if ((int)nodeStack.size() <= stackPos + 1)
                    nodeStack.resize(nodeStack.size() * 2, nullptr);

                test_case_hash_t hash = hashTestNodeName(curName, &hashCollisionDetectionMap);
                if (!nodeStack[stackPos + 1] || nodeStack[stackPos + 1]->getHash() != hash)
                {
                    CaseTreeNode *curGroup = nodeStack[stackPos]->getChild(hash);

                    if (!curGroup)
                    {
                        curGroup = new CaseTreeNode(hash);

                        try
                        {
                            nodeStack[stackPos]->addChild(curGroup);
                        }
                        catch (...)
                        {
                            delete curGroup;
                            throw;
                        }
                    }

                    nodeStack[stackPos + 1] = curGroup;

                    if ((int)nodeStack.size() > stackPos + 2)
                        nodeStack[stackPos + 2] = nullptr; // Invalidate rest of entries
                }

                DE_ASSERT(nodeStack[stackPos + 1]->getHash() == hash);

                curName.clear();
                stackPos += 1;
            }
            else if (isValidTestCaseNameChar((char)curChr))
                curName += (char)curChr;
            else
                throw std::invalid_argument("Illegal character in test case name");
        }
    }
}

static void parseCaseList(CaseTreeNode *root, std::istream &in, bool reportDuplicates,
                          std::unordered_map<test_case_hash_t, string> &hashCollisionDetectionMap)
{
    vector<CaseTreeNode *> nodeStack(8, root);
    parseSimpleCaseList(nodeStack, in, reportDuplicates, hashCollisionDetectionMap);
}

static void parseGroupFile(CaseTreeNode *root, std::istream &inGroupList, const tcu::Archive &archive,
                           bool reportDuplicates,
                           std::unordered_map<test_case_hash_t, string> &hashCollisionDetectionMap)
{
    // read whole file and remove all '\r'
    std::string buffer(std::istreambuf_iterator<char>(inGroupList), {});
    buffer.erase(std::remove(buffer.begin(), buffer.end(), '\r'), buffer.end());

    vector<CaseTreeNode *> nodeStack(8, root);
    std::stringstream namesStream(buffer);
    std::string fileName;

    while (std::getline(namesStream, fileName))
    {
        trimString(fileName);
        if (fileName.empty() || fileName.front() == '#') // Ignore empty lines and comments.
            continue;

        de::FilePath groupPath(fileName);
        de::UniquePtr<Resource> groupResource(archive.getResource(groupPath.normalize().getPath()));
        const int groupBufferSize(groupResource->getSize());
        std::vector<char> groupBuffer(static_cast<size_t>(groupBufferSize));

        groupResource->read(reinterpret_cast<uint8_t *>(&groupBuffer[0]), groupBufferSize);
        if (groupBuffer.empty())
            throw Exception("Empty case list resource");

        std::istringstream groupIn(std::string(groupBuffer.begin(), groupBuffer.end()));
        parseSimpleCaseList(nodeStack, groupIn, reportDuplicates, hashCollisionDetectionMap);
    }
}

static CaseTreeNode *parseCaseList(std::istream &in, const tcu::Archive &archive, const char *path = nullptr)
{
    std::unordered_map<test_case_hash_t, std::string> hashCollisionDetectionMap{};
    auto rootName            = "";
    test_case_hash_t hash    = hashTestNodeName(rootName, &hashCollisionDetectionMap);
    CaseTreeNode *const root = new CaseTreeNode(hash);
    try
    {
        if (in.peek() == '{')
            parseCaseTrie(root, in, hashCollisionDetectionMap);
        else
        {
            // if we are reading cases from file determine if we are
            // reading group file or plain list of cases; this is done by
            // reading single line and checking if it ends with ".txt"
            bool readGroupFile = false;
            if (path)
            {
                // read the first non-empty non-comment line and make sure it doesn't contain '\r'
                std::string line;
                while (std::getline(in, line))
                {
                    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
                    trimString(line);
                    if (!(line.empty() || line.front() == '#')) // Ignore empty lines and comments.
                        break;
                }

                const std::string ending = ".txt";
                readGroupFile =
                    (line.length() > ending.length()) && std::equal(ending.rbegin(), ending.rend(), line.rbegin());

                // move to the beginning of the file to parse first line too
                in.seekg(0, in.beg);
            }

            if (readGroupFile)
                parseGroupFile(root, in, archive, true, hashCollisionDetectionMap);
            else
                parseCaseList(root, in, true, hashCollisionDetectionMap);
        }

        {
            const int curChr = in.get();
            if (curChr != std::char_traits<char>::eof() && curChr != 0)
                throw std::invalid_argument("Trailing characters at end of case list");
        }

        return root;
    }
    catch (...)
    {
        delete root;
        throw;
    }
}

class CasePaths
{
public:
    CasePaths(const string &pathList);
    CasePaths(const vector<string> &pathList);
    bool matches(const string &caseName, bool allowPrefix = false) const;

private:
    const vector<string> m_casePatterns;
};

CasePaths::CasePaths(const string &pathList) : m_casePatterns(de::splitString(pathList, ','))
{
}

CasePaths::CasePaths(const vector<string> &pathList) : m_casePatterns(pathList)
{
}

// Match a single path component against a pattern component that may contain *-wildcards.
bool matchWildcards(string::const_iterator patternStart, string::const_iterator patternEnd,
                    string::const_iterator pathStart, string::const_iterator pathEnd, bool allowPrefix)
{
    string::const_iterator pattern = patternStart;
    string::const_iterator path    = pathStart;

    while (pattern != patternEnd && path != pathEnd && *pattern == *path)
    {
        ++pattern;
        ++path;
    }

    if (pattern == patternEnd)
        return (path == pathEnd);
    else if (*pattern == '*')
    {
        string::const_iterator patternNext = pattern + 1;
        if (patternNext != patternEnd)
        {
            for (; path != pathEnd; ++path)
            {
                if (*patternNext == *path)
                    if (matchWildcards(patternNext, patternEnd, path, pathEnd, allowPrefix))
                        return true;
            }
        }

        if (matchWildcards(pattern + 1, patternEnd, pathEnd, pathEnd, allowPrefix))
            return true;
    }
    else if (path == pathEnd && allowPrefix)
        return true;

    return false;
}

#if defined(TCU_HIERARCHICAL_CASEPATHS)
// Match a list of pattern components to a list of path components. A pattern
// component may contain *-wildcards. A pattern component "**" matches zero or
// more whole path components.
static bool patternMatches(vector<string>::const_iterator patternStart, vector<string>::const_iterator patternEnd,
                           vector<string>::const_iterator pathStart, vector<string>::const_iterator pathEnd,
                           bool allowPrefix)
{
    vector<string>::const_iterator pattern = patternStart;
    vector<string>::const_iterator path    = pathStart;

    while (pattern != patternEnd && path != pathEnd && *pattern != "**" &&
           (*pattern == *path || matchWildcards(pattern->begin(), pattern->end(), path->begin(), path->end(), false)))
    {
        ++pattern;
        ++path;
    }

    if (path == pathEnd && (allowPrefix || pattern == patternEnd))
        return true;
    else if (pattern != patternEnd && *pattern == "**")
    {
        for (; path != pathEnd; ++path)
            if (patternMatches(pattern + 1, patternEnd, path, pathEnd, allowPrefix))
                return true;
        if (patternMatches(pattern + 1, patternEnd, path, pathEnd, allowPrefix))
            return true;
    }

    return false;
}
#endif

bool CasePaths::matches(const string &caseName, bool allowPrefix) const
{
#if defined(TCU_HIERARCHICAL_CASEPATHS)
    const vector<string> components = de::splitString(caseName, '.');
#endif

    for (size_t ndx = 0; ndx < m_casePatterns.size(); ++ndx)
    {
#if defined(TCU_HIERARCHICAL_CASEPATHS)
        const vector<string> patternComponents = de::splitString(m_casePatterns[ndx], '.');

        if (patternMatches(patternComponents.begin(), patternComponents.end(), components.begin(), components.end(),
                           allowPrefix))
            return true;
#else
        if (matchWildcards(m_casePatterns[ndx].begin(), m_casePatterns[ndx].end(), caseName.begin(), caseName.end(),
                           allowPrefix))
            return true;
#endif
    }

    return false;
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct command line
 * \note CommandLine is not fully initialized until parse() has been called.
 *//*--------------------------------------------------------------------*/
CommandLine::CommandLine(void) : m_appName(), m_logFlags(0), m_hadHelpSpecified(false)
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct command line from standard argc, argv pair.
 *
 * Calls parse() with given arguments
 * \param archive application's assets
 * \param argc Number of arguments
 * \param argv Command line arguments
 *//*--------------------------------------------------------------------*/
CommandLine::CommandLine(int argc, const char *const *argv)
    : m_appName(argv[0])
    , m_logFlags(0)
    , m_hadHelpSpecified(false)
{
    if (argc > 1)
    {
        int loop = 1; // skip application name
        while (true)
        {
            m_initialCmdLine += std::string(argv[loop++]);
            if (loop >= argc)
                break;
            m_initialCmdLine += " ";
        }
    }

    if (!parse(argc, argv))
    {
        if (m_hadHelpSpecified)
            exit(EXIT_SUCCESS);
        else
            throw Exception("Failed to parse command line");
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct command line from string.
 *
 * Calls parse() with given argument.
 * \param archive application's assets
 * \param cmdLine Full command line string.
 *//*--------------------------------------------------------------------*/
CommandLine::CommandLine(const std::string &cmdLine) : m_appName(), m_initialCmdLine(cmdLine), m_hadHelpSpecified(false)
{
    if (!parse(cmdLine))
        throw Exception("Failed to parse command line");
}

CommandLine::~CommandLine(void)
{
}

void CommandLine::clear(void)
{
    m_cmdLine.clear();
    m_logFlags = 0;
}

const de::cmdline::CommandLine &CommandLine::getCommandLine(void) const
{
    return m_cmdLine;
}

const std::string &CommandLine::getApplicationName(void) const
{
    return m_appName;
}

const std::string &CommandLine::getInitialCmdLine(void) const
{
    return m_initialCmdLine;
}

void CommandLine::registerExtendedOptions(de::cmdline::Parser &parser)
{
    DE_UNREF(parser);
}

/*--------------------------------------------------------------------*//*!
 * \brief Parse command line from standard argc, argv pair.
 * \note parse() must be called exactly once.
 * \param argc Number of arguments
 * \param argv Command line arguments
 *//*--------------------------------------------------------------------*/
bool CommandLine::parse(int argc, const char *const *argv)
{
    DebugOutStreambuf sbuf;
    std::ostream debugOut(&sbuf);
    de::cmdline::Parser parser;

    opt::registerOptions(parser);
    opt::registerLegacyOptions(parser);
    registerExtendedOptions(parser);

    clear();

    if (!parser.parse(argc - 1, argv + 1, &m_cmdLine, std::cerr))
    {
        debugOut << "\n" << de::FilePath(argv[0]).getBaseName() << " [options]\n\n";
        parser.help(debugOut);

        // We need to save this to avoid exiting with error later, and before the clear() call that wipes its value.
        m_hadHelpSpecified = m_cmdLine.helpSpecified();

        clear();
        return false;
    }

    if (!m_cmdLine.getOption<opt::LogImages>())
        m_logFlags |= QP_TEST_LOG_EXCLUDE_IMAGES;

    if (m_cmdLine.getOption<opt::LogImages>() && m_cmdLine.getOption<opt::LogAllImages>())
        m_logFlags |= QP_TEST_LOG_ALL_IMAGES;

    if (!m_cmdLine.getOption<opt::LogShaderSources>())
        m_logFlags |= QP_TEST_LOG_EXCLUDE_SHADER_SOURCES;

    if (!m_cmdLine.getOption<opt::LogFlush>())
        m_logFlags |= QP_TEST_LOG_NO_FLUSH;

    if (m_cmdLine.getOption<opt::LogCompact>())
        m_logFlags |= QP_TEST_LOG_COMPACT;

    if (!m_cmdLine.getOption<opt::LogEmptyLoginfo>())
        m_logFlags |= QP_TEST_LOG_EXCLUDE_EMPTY_LOGINFO;

    if (m_cmdLine.getOption<opt::SubProcess>())
        m_logFlags |= QP_TEST_LOG_NO_INITIAL_OUTPUT;

    if ((m_cmdLine.hasOption<opt::CasePath>() ? 1 : 0) + (m_cmdLine.hasOption<opt::CaseList>() ? 1 : 0) +
            (m_cmdLine.hasOption<opt::CaseListFile>() ? 1 : 0) +
            (m_cmdLine.hasOption<opt::CaseListResource>() ? 1 : 0) +
            (m_cmdLine.getOption<opt::StdinCaseList>() ? 1 : 0) >
        1)
    {
        debugOut << "ERROR: multiple test case list options given!\n" << std::endl;
        clear();
        return false;
    }

    if (m_cmdLine.getArgs().size() > 0)
    {
        debugOut << "ERROR: arguments not starting with '-' or '--' are not supported by this application!\n"
                 << std::endl;

        debugOut << "\n" << de::FilePath(argv[0]).getBaseName() << " [options]\n\n";
        parser.help(debugOut);

        clear();
        return false;
    }

    return true;
}

/*--------------------------------------------------------------------*//*!
 * \brief Parse command line from string.
 * \note parse() must be called exactly once.
 * \param cmdLine Full command line string.
 *//*--------------------------------------------------------------------*/
bool CommandLine::parse(const std::string &cmdLine)
{
    deCommandLine *parsedCmdLine = deCommandLine_parse(cmdLine.c_str());
    if (!parsedCmdLine)
        throw std::bad_alloc();

    bool isOk = false;
    try
    {
        isOk = parse(parsedCmdLine->numArgs, parsedCmdLine->args);
    }
    catch (...)
    {
        deCommandLine_destroy(parsedCmdLine);
        throw;
    }

    deCommandLine_destroy(parsedCmdLine);
    return isOk;
}

bool CommandLine::quietMode(void) const
{
    return m_cmdLine.getOption<opt::QuietStdout>();
}
const char *CommandLine::getLogFileName(void) const
{
    return m_cmdLine.getOption<opt::LogFilename>().c_str();
}
uint32_t CommandLine::getLogFlags(void) const
{
    return m_logFlags;
}
RunMode CommandLine::getRunMode(void) const
{
    return m_cmdLine.getOption<opt::RunMode>();
}
const char *CommandLine::getCaseListExportFile(void) const
{
    return m_cmdLine.getOption<opt::ExportFilenamePattern>().c_str();
}
const char *CommandLine::getMustpassSpec(void) const
{
    return m_cmdLine.getOption<opt::MustpassSpec>().c_str();
}
WindowVisibility CommandLine::getVisibility(void) const
{
    return m_cmdLine.getOption<opt::Visibility>();
}
bool CommandLine::isWatchDogEnabled(void) const
{
    return m_cmdLine.getOption<opt::WatchDog>();
}
bool CommandLine::isCrashHandlingEnabled(void) const
{
    return m_cmdLine.getOption<opt::CrashHandler>();
}
int CommandLine::getBaseSeed(void) const
{
    return m_cmdLine.getOption<opt::BaseSeed>();
}
int CommandLine::getTestIterationCount(void) const
{
    return m_cmdLine.getOption<opt::TestIterationCount>();
}
int CommandLine::getSurfaceWidth(void) const
{
    return m_cmdLine.getOption<opt::SurfaceWidth>();
}
int CommandLine::getSurfaceHeight(void) const
{
    return m_cmdLine.getOption<opt::SurfaceHeight>();
}
SurfaceType CommandLine::getSurfaceType(void) const
{
    return m_cmdLine.getOption<opt::SurfaceType>();
}
ScreenRotation CommandLine::getScreenRotation(void) const
{
    return m_cmdLine.getOption<opt::ScreenRotation>();
}
int CommandLine::getGLConfigId(void) const
{
    return m_cmdLine.getOption<opt::GLConfigID>();
}
int CommandLine::getCLPlatformId(void) const
{
    return m_cmdLine.getOption<opt::CLPlatformID>();
}
const std::vector<int> &CommandLine::getCLDeviceIds(void) const
{
    return m_cmdLine.getOption<opt::CLDeviceIDs>();
}
int CommandLine::getVKDeviceId(void) const
{
    return m_cmdLine.getOption<opt::VKDeviceID>();
}
int CommandLine::getMaxCustomDevices(void) const
{
    return m_cmdLine.getOption<opt::MaxCustomDevices>();
}
int CommandLine::getVKDeviceGroupId(void) const
{
    return m_cmdLine.getOption<opt::VKDeviceGroupID>();
}
bool CommandLine::isValidationEnabled(void) const
{
    return m_cmdLine.getOption<opt::Validation>();
}
bool CommandLine::isSpirvValidationEnabled(void) const
{
    return m_cmdLine.getOption<opt::SpirvValidation>();
}
bool CommandLine::printValidationErrors(void) const
{
    return m_cmdLine.getOption<opt::PrintValidationErrors>();
}
bool CommandLine::checkDuplicateCaseNames(void) const
{
    return m_cmdLine.getOption<opt::DuplicateCheck>();
}
bool CommandLine::isLogDecompiledSpirvEnabled(void) const
{
    return m_cmdLine.getOption<opt::LogDecompiledSpirv>();
}
bool CommandLine::isOutOfMemoryTestEnabled(void) const
{
    return m_cmdLine.getOption<opt::TestOOM>();
}
bool CommandLine::isShadercacheEnabled(void) const
{
    return m_cmdLine.getOption<opt::ShaderCache>();
}
const char *CommandLine::getShaderCacheFilename(void) const
{
    return m_cmdLine.getOption<opt::ShaderCacheFilename>().c_str();
}
bool CommandLine::isShaderCacheTruncateEnabled(void) const
{
    return m_cmdLine.getOption<opt::ShaderCacheTruncate>();
}
bool CommandLine::isShaderCacheIPCEnabled(void) const
{
    return m_cmdLine.getOption<opt::ShaderCacheIPC>();
}
int CommandLine::getOptimizationRecipe(void) const
{
    return m_cmdLine.getOption<opt::Optimization>();
}
bool CommandLine::isSpirvOptimizationEnabled(void) const
{
    return m_cmdLine.getOption<opt::OptimizeSpirv>();
}
bool CommandLine::isRenderDocEnabled(void) const
{
    return m_cmdLine.getOption<opt::RenderDoc>();
}
const char *CommandLine::getWaiverFileName(void) const
{
    return m_cmdLine.getOption<opt::WaiverFile>().c_str();
}
const std::vector<int> &CommandLine::getCaseFraction(void) const
{
    return m_cmdLine.getOption<opt::CaseFraction>();
}
const char *CommandLine::getCaseFractionMandatoryTests(void) const
{
    return m_cmdLine.getOption<opt::CaseFractionMandatoryTests>().c_str();
}
const char *CommandLine::getArchiveDir(void) const
{
    return m_cmdLine.getOption<opt::ArchiveDir>().c_str();
}
tcu::TestRunnerType CommandLine::getRunnerType(void) const
{
    return m_cmdLine.getOption<opt::RunnerType>();
}
bool CommandLine::isTerminateOnFailEnabled(void) const
{
    return m_cmdLine.getOption<opt::TerminateOnFail>();
}
bool CommandLine::isTerminateOnDeviceLostEnabled(void) const
{
    return m_cmdLine.getOption<opt::TerminateOnDeviceLost>();
}
bool CommandLine::isSubProcess(void) const
{
    return m_cmdLine.getOption<opt::SubProcess>();
}
int CommandLine::getSubprocessTestCount(void) const
{
    return m_cmdLine.getOption<opt::SubprocessTestCount>();
}
int CommandLine::getCommandPoolMinSize(void) const
{
    return m_cmdLine.getOption<opt::CommandPoolMinSize>();
}
int CommandLine::getCommandBufferMinSize(void) const
{
    return m_cmdLine.getOption<opt::CommandBufferMinSize>();
}
int CommandLine::getCommandDefaultSize(void) const
{
    return m_cmdLine.getOption<opt::CommandDefaultSize>();
}
int CommandLine::getPipelineDefaultSize(void) const
{
    return m_cmdLine.getOption<opt::PipelineDefaultSize>();
}
bool CommandLine::isComputeOnly(void) const
{
    return m_cmdLine.getOption<opt::ComputeOnly>();
}
bool CommandLine::isVendorSpecific() const
{
    return m_cmdLine.getOption<opt::VendorSpecific>();
}

const char *CommandLine::getGLContextType(void) const
{
    if (m_cmdLine.hasOption<opt::GLContextType>())
        return m_cmdLine.getOption<opt::GLContextType>().c_str();
    else
        return nullptr;
}
const char *CommandLine::getGLConfigName(void) const
{
    if (m_cmdLine.hasOption<opt::GLConfigName>())
        return m_cmdLine.getOption<opt::GLConfigName>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getGLContextFlags(void) const
{
    if (m_cmdLine.hasOption<opt::GLContextFlags>())
        return m_cmdLine.getOption<opt::GLContextFlags>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getCLBuildOptions(void) const
{
    if (m_cmdLine.hasOption<opt::CLBuildOptions>())
        return m_cmdLine.getOption<opt::CLBuildOptions>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getEGLDisplayType(void) const
{
    if (m_cmdLine.hasOption<opt::EGLDisplayType>())
        return m_cmdLine.getOption<opt::EGLDisplayType>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getEGLWindowType(void) const
{
    if (m_cmdLine.hasOption<opt::EGLWindowType>())
        return m_cmdLine.getOption<opt::EGLWindowType>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getEGLPixmapType(void) const
{
    if (m_cmdLine.hasOption<opt::EGLPixmapType>())
        return m_cmdLine.getOption<opt::EGLPixmapType>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getSubprocessConfigFile(void) const
{
    if (m_cmdLine.hasOption<opt::SubprocessConfigFile>())
        return m_cmdLine.getOption<opt::SubprocessConfigFile>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getServerAddress(void) const
{
    if (m_cmdLine.hasOption<opt::ServerAddress>())
        return m_cmdLine.getOption<opt::ServerAddress>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getPipelineCompilerPath(void) const
{
    if (m_cmdLine.hasOption<opt::PipelineCompilerPath>())
        return m_cmdLine.getOption<opt::PipelineCompilerPath>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getPipelineCompilerDataDir(void) const
{
    if (m_cmdLine.hasOption<opt::PipelineCompilerDataDir>())
        return m_cmdLine.getOption<opt::PipelineCompilerDataDir>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getPipelineCompilerArgs(void) const
{
    if (m_cmdLine.hasOption<opt::PipelineCompilerArgs>())
        return m_cmdLine.getOption<opt::PipelineCompilerArgs>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getPipelineCompilerOutputFile(void) const
{
    if (m_cmdLine.hasOption<opt::PipelineCompilerOutputFile>())
        return m_cmdLine.getOption<opt::PipelineCompilerOutputFile>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getPipelineCompilerLogFile(void) const
{
    if (m_cmdLine.hasOption<opt::PipelineCompilerLogFile>())
        return m_cmdLine.getOption<opt::PipelineCompilerLogFile>().c_str();
    else
        return nullptr;
}

const char *CommandLine::getPipelineCompilerFilePrefix(void) const
{
    if (m_cmdLine.hasOption<opt::PipelineCompilerFilePrefix>())
        return m_cmdLine.getOption<opt::PipelineCompilerFilePrefix>().c_str();
    else
        return nullptr;
}

bool CommandLine::getVideoLogPrint(void) const
{
    return m_cmdLine.getOption<opt::VideoLogPrint>();
}

VideoDecodeOutput CommandLine::getVideoDumpDecodeOutput(void) const
{
    return m_cmdLine.getOption<opt::VideoDecodeOutputDump>();
}

VideoEncodeOutput CommandLine::getVideoDumpEncodeOutput(void) const
{
    return m_cmdLine.getOption<opt::VideoEncodeOutputDump>();
}

const char *CommandLine::getVkLibraryPath(void) const
{
    if (m_cmdLine.hasOption<opt::VkLibraryPath>())
        return (m_cmdLine.getOption<opt::VkLibraryPath>() != "") ? m_cmdLine.getOption<opt::VkLibraryPath>().c_str() :
                                                                   nullptr;
    else
        return nullptr;
}

const char *CommandLine::getAppParamsInputFilePath(void) const
{
    if (m_cmdLine.hasOption<opt::ApplicationParametersInputFile>())
        return m_cmdLine.getOption<opt::ApplicationParametersInputFile>().c_str();
    else
        return nullptr;
}

static bool checkTestGroupName(const CaseTreeNode *root, const char *groupPath)
{
    const CaseTreeNode *node = findNode(root, groupPath);
    return node && node->hasChildren();
}

static bool checkTestCaseName(const CaseTreeNode *root, const char *casePath)
{
    const CaseTreeNode *node = findNode(root, casePath);
    return node && !node->hasChildren();
}

de::MovePtr<CaseListFilter> CommandLine::createCaseListFilter(const tcu::Archive &archive) const
{
    return de::MovePtr<CaseListFilter>(new CaseListFilter(m_cmdLine, archive));
}

bool CaseListFilter::checkTestGroupName(const char *groupName) const
{
    bool result = false;
    if (m_casePaths)
        result = m_casePaths->matches(groupName, true);
    else if (m_caseTree)
        result = (groupName[0] == 0 || tcu::checkTestGroupName(m_caseTree, groupName));
    else
        return true;
    if (!result && m_caseFractionMandatoryTests.get() != nullptr)
        result = m_caseFractionMandatoryTests->matches(groupName, true);
    return result;
}

bool CaseListFilter::checkTestCaseName(const char *caseName) const
{
    bool result = false;
    if (m_casePaths)
        result = m_casePaths->matches(caseName, false);
    else if (m_caseTree)
        result = tcu::checkTestCaseName(m_caseTree, caseName);
    else
        result = true;
    if (!result && m_caseFractionMandatoryTests.get() != nullptr)
        result = m_caseFractionMandatoryTests->matches(caseName, false);

    if (result && m_excludePaths && m_excludePaths->matches(caseName, false))
        result = false;

    return result;
}

bool CaseListFilter::checkCaseFraction(int i, const std::string &testCaseName, bool useFraction0) const
{
    // If the fraction is not set, run the test
    if (m_caseFraction.size() != 2)
        return true;

    // If the test is mandatory, run it.
    if (m_caseFractionMandatoryTests.get() != nullptr && m_caseFractionMandatoryTests->matches(testCaseName))
        return true;

    // If this test must run in fraction zero, and this is fraction zero, run it.
    if (useFraction0 && m_caseFraction[0] == 0)
        return true;

    // Split according to group index
    if (!useFraction0 && (i % m_caseFraction[1]) == m_caseFraction[0])
        return true;

    return false;
}

CaseListFilter::CaseListFilter(void) : m_caseTree(nullptr), m_runnerType(tcu::RUNNERTYPE_ANY)
{
}

CaseListFilter::CaseListFilter(const de::cmdline::CommandLine &cmdLine, const tcu::Archive &archive)
    : m_caseTree(nullptr)
{
    if (cmdLine.getOption<opt::RunMode>() == RUNMODE_VERIFY_AMBER_COHERENCY)
    {
        m_runnerType = RUNNERTYPE_AMBER;
    }
    else
    {
        m_runnerType = cmdLine.getOption<opt::RunnerType>();
    }

    if (cmdLine.hasOption<opt::CaseList>())
    {
        std::istringstream str(cmdLine.getOption<opt::CaseList>());

        m_caseTree = parseCaseList(str, archive);
    }
    else if (cmdLine.hasOption<opt::CaseListFile>())
    {
        std::string caseListFile = cmdLine.getOption<opt::CaseListFile>();
        std::ifstream in(caseListFile.c_str(), std::ios_base::binary);

        if (!in.is_open() || !in.good())
            throw Exception("Failed to open case list file '" + caseListFile + "'");

        m_caseTree = parseCaseList(in, archive, caseListFile.c_str());
    }
    else if (cmdLine.hasOption<opt::CaseListResource>())
    {
        // \todo [2016-11-14 pyry] We are cloning potentially large buffers here. Consider writing
        //                           istream adaptor for tcu::Resource.
        de::UniquePtr<Resource> caseListResource(
            archive.getResource(cmdLine.getOption<opt::CaseListResource>().c_str()));
        const int bufferSize = caseListResource->getSize();
        std::vector<char> buffer((size_t)bufferSize);

        if (buffer.empty())
            throw Exception("Empty case list resource");

        caseListResource->read(reinterpret_cast<uint8_t *>(&buffer[0]), bufferSize);

        {
            std::istringstream in(std::string(&buffer[0], (size_t)bufferSize));

            m_caseTree = parseCaseList(in, archive);
        }
    }
    else if (cmdLine.getOption<opt::StdinCaseList>())
    {
        m_caseTree = parseCaseList(std::cin, archive);
    }
    else if (cmdLine.hasOption<opt::CasePath>())
        m_casePaths = de::MovePtr<const CasePaths>(new CasePaths(cmdLine.getOption<opt::CasePath>()));

    if (cmdLine.hasOption<opt::ExcludeCasePath>())
        m_excludePaths = de::MovePtr<const CasePaths>(new CasePaths(cmdLine.getOption<opt::ExcludeCasePath>()));

    if (!cmdLine.getOption<opt::SubProcess>())
        m_caseFraction = cmdLine.getOption<opt::CaseFraction>();

    if (m_caseFraction.size() == 2 &&
        (m_caseFraction[0] < 0 || m_caseFraction[1] <= 0 || m_caseFraction[0] >= m_caseFraction[1]))
        throw Exception("Invalid case fraction. First element must be non-negative and less than second element. "
                        "Second element must be greater than 0.");

    if (m_caseFraction.size() != 0 && m_caseFraction.size() != 2)
        throw Exception("Invalid case fraction. Must have two components.");

    if (m_caseFraction.size() == 2)
    {
        std::string caseFractionMandatoryTestsFilename = cmdLine.getOption<opt::CaseFractionMandatoryTests>();

        if (!caseFractionMandatoryTestsFilename.empty())
        {
            std::ifstream fileStream(caseFractionMandatoryTestsFilename.c_str(), std::ios_base::binary);
            if (!fileStream.is_open() || !fileStream.good())
                throw Exception("Failed to open case fraction mandatory test list: '" +
                                caseFractionMandatoryTestsFilename + "'");

            std::vector<std::string> cfPaths;
            std::string line;

            while (std::getline(fileStream, line))
            {
                line.erase(std::remove(std::begin(line), std::end(line), '\r'), std::end(line));
                cfPaths.push_back(line);
            }
            if (!cfPaths.empty())
            {
                m_caseFractionMandatoryTests = de::MovePtr<const CasePaths>(new CasePaths(cfPaths));
                if (m_caseTree != nullptr)
                {
                    fileStream.clear();
                    fileStream.seekg(0, fileStream.beg);
                    std::unordered_map<test_case_hash_t, std::string> hashCollisionDetectionMap{};
                    parseCaseList(m_caseTree, fileStream, false, hashCollisionDetectionMap);
                }
            }
        }
    }
}

CaseListFilter::~CaseListFilter(void)
{
    delete m_caseTree;
}

} // namespace tcu
