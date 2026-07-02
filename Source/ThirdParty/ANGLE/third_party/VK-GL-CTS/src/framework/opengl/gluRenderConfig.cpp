/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief OpenGL rendering configuration.
 *//*--------------------------------------------------------------------*/

#include "gluRenderConfig.hpp"
#include "tcuCommandLine.hpp"
#include "deString.h"
#include "eglwEnums.hpp"

namespace glu
{

void parseConfigBitsFromName(RenderConfig *config, const char *renderCfgName)
{
    const char *cfgName = renderCfgName;

    DE_ASSERT(config->redBits == RenderConfig::DONT_CARE && config->greenBits == RenderConfig::DONT_CARE &&
              config->blueBits == RenderConfig::DONT_CARE && config->alphaBits == RenderConfig::DONT_CARE &&
              config->depthBits == RenderConfig::DONT_CARE && config->stencilBits == RenderConfig::DONT_CARE &&
              config->numSamples == RenderConfig::DONT_CARE);

    static const struct
    {
        const char *name;
        int redBits;
        int greenBits;
        int blueBits;
        int alphaBits;
    } colorCfgs[] = {{"rgb888", 8, 8, 8, 0},
                     {"rgba8888", 8, 8, 8, 8},
                     {"rgb565", 5, 6, 5, 0},
                     {"rgba4444", 4, 4, 4, 4},
                     {"rgba5551", 5, 5, 5, 1}};
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(colorCfgs); ndx++)
    {
        if (deStringBeginsWith(cfgName, colorCfgs[ndx].name))
        {
            config->redBits   = colorCfgs[ndx].redBits;
            config->greenBits = colorCfgs[ndx].greenBits;
            config->blueBits  = colorCfgs[ndx].blueBits;
            config->alphaBits = colorCfgs[ndx].alphaBits;

            cfgName += strlen(colorCfgs[ndx].name);
            break;
        }
    }

    static const struct
    {
        const char *name;
        int depthSize;
    } depthCfgs[] = {{"d0", 0}, {"d16", 16}, {"d24", 24}, {"d32", 32}};
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(depthCfgs); ndx++)
    {
        if (deStringBeginsWith(cfgName, depthCfgs[ndx].name))
        {
            config->depthBits = depthCfgs[ndx].depthSize;

            cfgName += strlen(depthCfgs[ndx].name);
            break;
        }
    }

    static const struct
    {
        const char *name;
        int stencilSize;
    } stencilCfgs[] = {
        {"s0", 0},
        {"s8", 8},
        {"s16", 16},
    };
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(stencilCfgs); ndx++)
    {
        if (deStringBeginsWith(cfgName, stencilCfgs[ndx].name))
        {
            config->stencilBits = stencilCfgs[ndx].stencilSize;

            cfgName += strlen(stencilCfgs[ndx].name);
            break;
        }
    }

    static const struct
    {
        const char *name;
        int numSamples;
    } multiSampleCfgs[] = {{"ms0", 0}, {"ms16", 16}, {"ms1", 1}, {"ms2", 2}, {"ms4", 4}, {"ms8", 8}};
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(multiSampleCfgs); ndx++)
    {
        if (deStringBeginsWith(cfgName, multiSampleCfgs[ndx].name))
        {
            config->numSamples = multiSampleCfgs[ndx].numSamples;

            cfgName += strlen(multiSampleCfgs[ndx].name);
            break;
        }
    }

    if (cfgName[0] != 0)
        throw tcu::InternalError(std::string("Invalid GL configuration: '") + renderCfgName + "'");
}

void parseRenderConfig(RenderConfig *config, const tcu::CommandLine &cmdLine)
{
    switch (cmdLine.getSurfaceType())
    {
    case tcu::SURFACETYPE_WINDOW:
        config->surfaceType = RenderConfig::SURFACETYPE_WINDOW;
        break;
    case tcu::SURFACETYPE_OFFSCREEN_NATIVE:
        config->surfaceType = RenderConfig::SURFACETYPE_OFFSCREEN_NATIVE;
        break;
    case tcu::SURFACETYPE_OFFSCREEN_GENERIC:
        config->surfaceType = RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC;
        break;
    case tcu::SURFACETYPE_FBO:
        config->surfaceType = RenderConfig::SURFACETYPE_DONT_CARE;
        break;
    case tcu::SURFACETYPE_LAST:
        config->surfaceType = RenderConfig::SURFACETYPE_DONT_CARE;
        break;
    default:
        throw tcu::InternalError("Unsupported surface type");
    }

    config->windowVisibility = parseWindowVisibility(cmdLine);

    if (cmdLine.getSurfaceWidth() > 0)
        config->width = cmdLine.getSurfaceWidth();

    if (cmdLine.getSurfaceHeight() > 0)
        config->height = cmdLine.getSurfaceHeight();

    if (cmdLine.getGLConfigName() != nullptr)
        parseConfigBitsFromName(config, cmdLine.getGLConfigName());

    if (cmdLine.getGLConfigId() >= 0)
        config->id = cmdLine.getGLConfigId();
}

RenderConfig::Visibility parseWindowVisibility(const tcu::CommandLine &cmdLine)
{
    switch (cmdLine.getVisibility())
    {
    case tcu::WINDOWVISIBILITY_HIDDEN:
        return RenderConfig::VISIBILITY_HIDDEN;
    case tcu::WINDOWVISIBILITY_WINDOWED:
        return RenderConfig::VISIBILITY_VISIBLE;
    case tcu::WINDOWVISIBILITY_FULLSCREEN:
        return RenderConfig::VISIBILITY_FULLSCREEN;
    default:
        throw tcu::InternalError("Unsupported window visibility");
    }
}

RenderConfig::ComponentType fromEGLComponentType(int eglComponentType)
{
    switch (eglComponentType)
    {
    case EGL_NONE:
        return glu::RenderConfig::COMPONENT_TYPE_DONT_CARE;
    case EGL_DONT_CARE:
        return glu::RenderConfig::COMPONENT_TYPE_DONT_CARE;
    case EGL_COLOR_COMPONENT_TYPE_FIXED_EXT:
        return glu::RenderConfig::COMPONENT_TYPE_FIXED;
    case EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT:
        return glu::RenderConfig::COMPONENT_TYPE_FLOAT;
    default:
        throw tcu::InternalError("Unsupported color component type");
    }
}

int toEGLComponentType(RenderConfig::ComponentType gluComponentType)
{
    switch (gluComponentType)
    {
    case glu::RenderConfig::COMPONENT_TYPE_DONT_CARE:
        return EGL_DONT_CARE;
    case glu::RenderConfig::COMPONENT_TYPE_FIXED:
        return EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;
    case glu::RenderConfig::COMPONENT_TYPE_FLOAT:
        return EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT;
    default:
        throw tcu::InternalError("Unsupported color component type");
    }
}

} // namespace glu
