#ifndef _TCULNXWAYLAND_HPP
#define _TCULNXWAYLAND_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2014 The Android Open Source Project
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Mun Gwan-gyeong <elongbug@gmail.com>
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
 * \brief wayland utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluRenderConfig.hpp"
#include "gluPlatform.hpp"
#include "tcuLnx.hpp"

#include <wayland-client.h>
#include <wayland-egl.h>
#include "xdg-shell.h"

namespace tcu
{
namespace lnx
{
namespace wayland
{

class Display
{
public:
    Display(EventState &platform, const char *name);
    virtual ~Display(void);

    struct wl_display *getDisplay(void)
    {
        return m_display;
    }
    struct wl_compositor *getCompositor(void)
    {
        return m_compositor;
    }
    struct xdg_wm_base *getShell(void)
    {
        return m_shell;
    }

    void processEvents(void);
    static bool hasDisplay(const char *name);

    enum DisplayState
    {
        DISPLAY_STATE_UNKNOWN = -1,
        DISPLAY_STATE_UNAVAILABLE,
        DISPLAY_STATE_AVAILABLE
    };
    static DisplayState s_displayState;

protected:
    EventState &m_eventState;
    struct wl_display *m_display;
    struct wl_registry *m_registry;
    struct wl_compositor *m_compositor;
    struct xdg_wm_base *m_shell;

private:
    Display(const Display &);
    Display &operator=(const Display &);

    static const struct wl_registry_listener s_registryListener;

    static void handleGlobal(void *data, struct wl_registry *registry, uint32_t id, const char *interface,
                             uint32_t version);
    static void handleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name);
};

class Window
{
public:
    Window(Display &display, int width, int height);
    ~Window(void);

    void setVisibility(bool visible);

    void processEvents(void);
    Display &getDisplay(void)
    {
        return m_display;
    }
    void *getSurface(void)
    {
        return m_surface;
    }
    void *getWindow(void)
    {
        return m_window;
    }

    void getDimensions(int *width, int *height) const;
    void setDimensions(int width, int height);

protected:
    Display &m_display;
    struct wl_egl_window *m_window;
    struct wl_surface *m_surface;
    struct xdg_surface *m_xdgSurface;
    struct xdg_toplevel *m_topLevel;
    bool m_configured;
    bool m_visible;

private:
    Window(const Window &);
    Window &operator=(const Window &);

    static const struct xdg_surface_listener s_xdgSurfaceListener;
    static const struct xdg_wm_base_listener s_wmBaseListener;

    static void handlePing(void *data, struct xdg_wm_base *shellSurface, uint32_t serial);
    static void handleConfigure(void *data, struct xdg_surface *shellSurface, uint32_t serial);

    static bool s_addWMBaseListener;
};

} // namespace wayland
} // namespace lnx
} // namespace tcu

#endif // _TCULNXWAYLAND_HPP
