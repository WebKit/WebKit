/*-------------------------------------------------------------------------
 * drawElements Quality Program Platform Utilites
 * ----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Android platform capability query JNI component
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

#include "tcuCommandLine.hpp"
#include "gluRenderConfig.hpp"
#include "gluRenderContext.hpp"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"
#include "egluUtil.hpp"
#include "egluGLUtil.hpp"

#include <jni.h>

namespace
{
namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(GLMajorVersion, int);
DE_DECLARE_COMMAND_LINE_OPT(GLMinorVersion, int);

} // namespace opt

class GLConfigParser : public tcu::CommandLine
{
public:
    GLConfigParser(const std::string &argString);

    bool hasGLMajorVersion(void) const;
    bool hasGLMinorVersion(void) const;
    int getGLMajorVersion(void) const;
    int getGLMinorVersion(void) const;

private:
    virtual void registerExtendedOptions(de::cmdline::Parser &parser);
};

GLConfigParser::GLConfigParser(const std::string &argString)
{
    const std::string execString = "fakebinaryname " + argString; // convert argument list to full command line

    if (!parse(execString))
    {
        tcu::print("failed to parse command line");
        TCU_THROW(Exception, "failed to parse command line");
    }
}

bool GLConfigParser::hasGLMajorVersion(void) const
{
    return getCommandLine().hasOption<opt::GLMajorVersion>();
}

bool GLConfigParser::hasGLMinorVersion(void) const
{
    return getCommandLine().hasOption<opt::GLMinorVersion>();
}

int GLConfigParser::getGLMajorVersion(void) const
{
    DE_ASSERT(hasGLMajorVersion());
    return getCommandLine().getOption<opt::GLMajorVersion>();
}

int GLConfigParser::getGLMinorVersion(void) const
{
    DE_ASSERT(hasGLMinorVersion());
    return getCommandLine().getOption<opt::GLMinorVersion>();
}

void GLConfigParser::registerExtendedOptions(de::cmdline::Parser &parser)
{
    using de::cmdline::Option;

    parser << Option<opt::GLMajorVersion>(nullptr, "deqp-gl-major-version", "OpenGL ES Major version")
           << Option<opt::GLMinorVersion>(nullptr, "deqp-gl-minor-version", "OpenGL ES Minor version");
}

glu::RenderConfig parseRenderConfig(const std::string &argsStr)
{
    const GLConfigParser parsedCommandLine(argsStr);

    if (!parsedCommandLine.hasGLMajorVersion() || !parsedCommandLine.hasGLMinorVersion())
    {
        tcu::print("minor and major version must be supplied");
        TCU_THROW(Exception, "minor and major version must be supplied");
    }
    else
    {
        const glu::ContextType testContextType(
            glu::ApiType::es(parsedCommandLine.getGLMajorVersion(), parsedCommandLine.getGLMinorVersion()));
        glu::RenderConfig renderConfig(testContextType);

        glu::parseRenderConfig(&renderConfig, parsedCommandLine);

        return renderConfig;
    }
}

bool isRenderConfigSupported(const std::string &cmdLineStr)
{
    const glu::RenderConfig renderConfig = parseRenderConfig(cmdLineStr);
    const eglw::DefaultLibrary egl;
    const eglw::EGLDisplay display = egl.getDisplay(EGL_DEFAULT_DISPLAY);
    eglw::EGLint eglMajor          = -1;
    eglw::EGLint eglMinor          = -1;

    if (display == EGL_NO_DISPLAY)
    {
        tcu::print("could not get default display");
        TCU_THROW(Exception, "could not get default display");
    }

    if (egl.initialize(display, &eglMajor, &eglMinor) != EGL_TRUE)
    {
        tcu::print("failed to initialize egl");
        TCU_THROW(Exception, "failed to initialize egl");
    }
    tcu::print("EGL initialized, major=%d, minor=%d", eglMajor, eglMinor);

    try
    {
        // ignoring return value
        (void)eglu::chooseConfig(egl, display, renderConfig);
    }
    catch (const tcu::NotSupportedError &)
    {
        tcu::print("No matching config");
        egl.terminate(display);
        return false;
    }
    catch (...)
    {
        egl.terminate(display);
        throw;
    }
    egl.terminate(display);

    return true;
}

} // namespace

DE_BEGIN_EXTERN_C

JNIEXPORT jint JNICALL
Java_com_drawelements_deqp_platformutil_DeqpPlatformCapabilityQueryInstrumentation_nativeRenderConfigSupportedQuery(
    JNIEnv *env, jclass, jstring jCmdLine)
{
    enum
    {
        CONFIGQUERYRESULT_SUPPORTED     = 0,
        CONFIGQUERYRESULT_NOT_SUPPORTED = 1,
        CONFIGQUERYRESULT_GENERIC_ERROR = -1,
    };

    std::string cmdLine;
    const char *const cmdLineBytes = env->GetStringUTFChars(jCmdLine, nullptr);

    if (cmdLineBytes == nullptr)
    {
        // no command line is not executable
        tcu::print("no command line supplied");
        return CONFIGQUERYRESULT_GENERIC_ERROR;
    }

    try
    {
        // try to copy to local buffer
        cmdLine = std::string(cmdLineBytes);
    }
    catch (const std::bad_alloc &)
    {
        env->ReleaseStringUTFChars(jCmdLine, cmdLineBytes);
        tcu::print("failed to copy cmdLine");
        return CONFIGQUERYRESULT_GENERIC_ERROR;
    }
    env->ReleaseStringUTFChars(jCmdLine, cmdLineBytes);

    try
    {
        const bool isSupported = isRenderConfigSupported(cmdLine);

        return (isSupported) ? (CONFIGQUERYRESULT_SUPPORTED) : (CONFIGQUERYRESULT_NOT_SUPPORTED);
    }
    catch (const std::exception &ex)
    {
        // don't bother forwarding the exception to the caller. They cannot do anything with the exception anyway.
        tcu::print("Error: %s", ex.what());
        return CONFIGQUERYRESULT_GENERIC_ERROR;
    }
}

DE_END_EXTERN_C
