#ifndef _GLURENDERCONFIG_HPP
#define _GLURENDERCONFIG_HPP
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

#include "tcuDefs.hpp"
#include "gluRenderContext.hpp"

namespace tcu
{
class CommandLine;
}

namespace glu
{

enum ResetNotificationStrategy
{
    RESET_NOTIFICATION_STRATEGY_NOT_SPECIFIED = 0,     //!< Not specified, implementation-dependent
    RESET_NOTIFICATION_STRATEGY_NO_RESET_NOTIFICATION, //!< No reset notification (may not be supported)
    RESET_NOTIFICATION_STRATEGY_LOSE_CONTEXT_ON_RESET, //!< Lose context on reset (may not be supported)

    RESET_NOTIFICATION_STRATEGY_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief Rendering context configuration.
 *//*--------------------------------------------------------------------*/
struct RenderConfig
{
    enum SurfaceType
    {
        SURFACETYPE_DONT_CARE = 0,
        SURFACETYPE_WINDOW,            //!< Native window.
        SURFACETYPE_OFFSCREEN_NATIVE,  //!< Native renderable offscreen buffer, such as pixmap or bitmap.
        SURFACETYPE_OFFSCREEN_GENERIC, //!< Generic offscreen buffer, such as EGL pbuffer.

        SURFACETYPE_LAST
    };

    enum Visibility
    {
        VISIBILITY_HIDDEN = 0,
        VISIBILITY_VISIBLE,
        VISIBILITY_FULLSCREEN,

        VISIBILITY_LAST
    };

    enum ComponentType
    {
        COMPONENT_TYPE_DONT_CARE = 0,
        COMPONENT_TYPE_FIXED,
        COMPONENT_TYPE_FLOAT,

        COMPONENT_TYPE_LAST
    };

    enum
    {
        DONT_CARE = -1
    };

    ContextType type;

    int width;
    int height;
    SurfaceType surfaceType;
    Visibility windowVisibility;
    ComponentType componentType;

    int id;

    int redBits;
    int greenBits;
    int blueBits;
    int alphaBits;
    int depthBits;
    int stencilBits;
    int numSamples;

    ResetNotificationStrategy resetNotificationStrategy;

    RenderConfig(ContextType type_ = ContextType())
        : type(type_)
        , width(DONT_CARE)
        , height(DONT_CARE)
        , surfaceType(SURFACETYPE_DONT_CARE)
        , windowVisibility(VISIBILITY_VISIBLE)
        , componentType(COMPONENT_TYPE_DONT_CARE)
        , id(DONT_CARE)
        , redBits(DONT_CARE)
        , greenBits(DONT_CARE)
        , blueBits(DONT_CARE)
        , alphaBits(DONT_CARE)
        , depthBits(DONT_CARE)
        , stencilBits(DONT_CARE)
        , numSamples(DONT_CARE)
        , resetNotificationStrategy(RESET_NOTIFICATION_STRATEGY_NOT_SPECIFIED)
    {
    }
} DE_WARN_UNUSED_TYPE;

// Utilities

void parseRenderConfig(RenderConfig *config, const tcu::CommandLine &cmdLine);
RenderConfig::Visibility parseWindowVisibility(const tcu::CommandLine &cmdLine);

template <typename T>
T getValueOrDefault(const RenderConfig &config, const T RenderConfig::*field, T defaultValue)
{
    T value = config.*field;
    return value == (T)RenderConfig::DONT_CARE ? defaultValue : value;
}

RenderConfig::ComponentType fromEGLComponentType(int eglComponentType);
int toEGLComponentType(RenderConfig::ComponentType gluComponentType);

} // namespace glu

#endif // _GLURENDERCONFIG_HPP
