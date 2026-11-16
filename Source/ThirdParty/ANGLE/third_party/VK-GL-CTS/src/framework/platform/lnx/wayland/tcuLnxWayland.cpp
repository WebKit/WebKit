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

#include "tcuLnxWayland.hpp"
#include "gluRenderConfig.hpp"
#include "deMemory.h"

#include <stdio.h>

namespace tcu
{
namespace lnx
{
namespace wayland
{

const struct wl_registry_listener Display::s_registryListener
{
    Display::handleGlobal, Display::handleGlobalRemove
};

Display::DisplayState Display::s_displayState = Display::DISPLAY_STATE_UNKNOWN;

bool Window::s_addWMBaseListener = true;

const struct xdg_surface_listener Window::s_xdgSurfaceListener
{
    Window::handleConfigure
};

const struct xdg_wm_base_listener Window::s_wmBaseListener
{
    Window::handlePing
};

void Display::handleGlobal(void *data, struct wl_registry *registry, uint32_t id, const char *interface,
                           uint32_t version)
{
    Display *_this = static_cast<Display *>(data);
    DE_UNREF(version);

    if (!strcmp(interface, "wl_compositor"))
        _this->m_compositor = static_cast<struct wl_compositor *>(
            wl_registry_bind(registry, id, &wl_compositor_interface, version > 3 ? version : 3));
    if (!strcmp(interface, "xdg_wm_base"))
        _this->m_shell = static_cast<struct xdg_wm_base *>(wl_registry_bind(registry, id, &xdg_wm_base_interface, 1));
}

void Display::handleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name)
{
    DE_UNREF(data);
    DE_UNREF(registry);
    DE_UNREF(name);
}

bool Display::hasDisplay(const char *name)
{
    if (s_displayState == DISPLAY_STATE_UNKNOWN)
    {
        struct wl_display *display = wl_display_connect(name);
        if (display)
        {
            s_displayState = DISPLAY_STATE_AVAILABLE;
            wl_display_disconnect(display);
        }
        else
            s_displayState = DISPLAY_STATE_UNAVAILABLE;
    }
    return s_displayState == DISPLAY_STATE_AVAILABLE ? true : false;
}

Display::Display(EventState &eventState, const char *name)
    : m_eventState(eventState)
    , m_display(nullptr)
    , m_registry(nullptr)
    , m_compositor(nullptr)
    , m_shell(nullptr)
{
    try
    {
        m_display = wl_display_connect(name);
        if (!m_display)
            throw ResourceError("Failed to open display", name, __FILE__, __LINE__);

        m_registry = wl_display_get_registry(m_display);
        if (!m_registry)
            throw ResourceError("Failed to get registry", name, __FILE__, __LINE__);

        wl_registry_add_listener(m_registry, &s_registryListener, this);
        wl_display_roundtrip(m_display);
        if (!m_compositor)
            throw ResourceError("Failed to bind compositor", name, __FILE__, __LINE__);
        if (!m_shell)
            throw ResourceError("Failed to bind shell", name, __FILE__, __LINE__);
    }
    catch (...)
    {
        if (m_shell)
            xdg_wm_base_destroy(m_shell);

        if (m_compositor)
            wl_compositor_destroy(m_compositor);

        if (m_registry)
            wl_registry_destroy(m_registry);

        if (m_display)
            wl_display_disconnect(m_display);

        throw;
    }
}

Display::~Display(void)
{
    if (m_shell)
        xdg_wm_base_destroy(m_shell);

    if (m_compositor)
        wl_compositor_destroy(m_compositor);

    if (m_registry)
        wl_registry_destroy(m_registry);

    if (m_display)
        wl_display_disconnect(m_display);
}

void Display::processEvents(void)
{
}

Window::Window(Display &display, int width, int height) : m_display(display)
{
    try
    {
        m_surface = wl_compositor_create_surface(display.getCompositor());
        if (!m_surface)
            throw ResourceError("Failed to create ", "surface", __FILE__, __LINE__);

        m_xdgSurface = xdg_wm_base_get_xdg_surface(display.getShell(), m_surface);
        if (!m_xdgSurface)
            throw ResourceError("Failed to create ", "shell_surface", __FILE__, __LINE__);

        // add wm base listener once
        if (s_addWMBaseListener)
        {
            xdg_wm_base_add_listener(display.getShell(), &s_wmBaseListener, this);
            s_addWMBaseListener = false;
        }
        xdg_surface_add_listener(m_xdgSurface, &s_xdgSurfaceListener, this);

        // select xdg surface role
        m_topLevel = xdg_surface_get_toplevel(m_xdgSurface);
        xdg_toplevel_set_title(m_topLevel, "CTS for OpenGL (ES)");

        // configure xdg surface
        m_configured = false;
        wl_surface_commit(m_surface);

        // wait till xdg surface is configured
        int dispatchedEvents = 0;
        while (dispatchedEvents != -1)
        {
            dispatchedEvents = wl_display_dispatch(display.getDisplay());
            if (m_configured)
                break;
        }

        if (width == glu::RenderConfig::DONT_CARE)
            width = DEFAULT_WINDOW_WIDTH;
        if (height == glu::RenderConfig::DONT_CARE)
            height = DEFAULT_WINDOW_HEIGHT;

        m_window = wl_egl_window_create(m_surface, width, height);
        if (!m_window)
            throw ResourceError("Failed to create ", "window", __FILE__, __LINE__);
    }
    catch (...)
    {
        throw;
    }
    TCU_CHECK(m_window);
}

void Window::setVisibility(bool visible)
{
    m_visible = visible;
}

void Window::getDimensions(int *width, int *height) const
{
    wl_egl_window_get_attached_size(m_window, width, height);
}

void Window::setDimensions(int width, int height)
{
    wl_egl_window_resize(m_window, width, height, 0, 0);
}

void Window::processEvents(void)
{
}

void Window::handlePing(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
    DE_UNREF(data);
    xdg_wm_base_pong(shell, serial);
}

void Window::handleConfigure(void *data, struct xdg_surface *xdgSurface, uint32_t serial)
{
    Window *window       = reinterpret_cast<Window *>(data);
    window->m_configured = true;

    xdg_surface_ack_configure(xdgSurface, serial);
}

Window::~Window(void)
{
    if (m_window)
        wl_egl_window_destroy(m_window);
    if (m_topLevel)
        xdg_toplevel_destroy(m_topLevel);
    if (m_xdgSurface)
        xdg_surface_destroy(m_xdgSurface);
    if (m_surface)
        wl_surface_destroy(m_surface);
}

} // namespace wayland
} // namespace lnx
} // namespace tcu
