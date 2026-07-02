/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief X11 using XCB utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuLnxX11Xcb.hpp"
#include "deMemory.h"

namespace tcu
{
namespace lnx
{
namespace x11
{

XcbDisplay::DisplayState XcbDisplay::s_displayState = XcbDisplay::DISPLAY_STATE_UNKNOWN;

bool XcbDisplay::hasDisplay(const char *name)
{
    if (s_displayState == DISPLAY_STATE_UNKNOWN)
    {
        xcb_connection_t *connection = xcb_connect(name, NULL);
        if (connection && !xcb_connection_has_error(connection))
        {
            s_displayState = DISPLAY_STATE_AVAILABLE;
            xcb_disconnect(connection);
        }
        else
        {
            s_displayState = DISPLAY_STATE_UNAVAILABLE;
        }
    }
    return s_displayState == DISPLAY_STATE_AVAILABLE ? true : false;
}

XcbDisplay::XcbDisplay(EventState &platform, const char *name) : DisplayBase(platform)
{
    m_connection                   = xcb_connect(name, NULL);
    const xcb_setup_t *setup       = xcb_get_setup(m_connection);
    xcb_screen_iterator_t iterator = xcb_setup_roots_iterator(setup);
    m_screen                       = iterator.data;
}

XcbDisplay::~XcbDisplay(void)
{
    xcb_disconnect(m_connection);
}

void XcbDisplay::processEvents(void)
{
    xcb_generic_event_t *ev;
    while ((ev = xcb_poll_for_event(m_connection)))
    {
        deFree(ev);
        /* Manage your event */
    }
}

XcbWindow::XcbWindow(XcbDisplay &display, int width, int height, xcb_visualid_t *visual)
    : WindowBase()
    , m_display(display)
{
    xcb_connection_t *connection = m_display.getConnection();
    uint32_t values[2];
    m_window   = xcb_generate_id(connection);
    m_colormap = xcb_generate_id(connection);

    if (visual == nullptr)
        visual = &m_display.getScreen()->root_visual;

    values[0] = m_display.getScreen()->white_pixel;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_PROPERTY_CHANGE;

    xcb_create_window(connection,                            // Connection
                      XCB_COPY_FROM_PARENT,                  // depth (same as root)
                      m_window,                              // window Id
                      display.getScreen()->root,             // parent window
                      0, 0,                                  // x, y
                      static_cast<uint16_t>(width),          // width
                      static_cast<uint16_t>(height),         // height
                      10,                                    // border_width
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,         // class
                      *visual,                               // visual
                      XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, // masks
                      values                                 //not used yet
    );

    xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, m_colormap, m_window, *visual);

    xcb_alloc_color_reply_t *rep =
        xcb_alloc_color_reply(connection, xcb_alloc_color(connection, m_colormap, 65535, 0, 0), NULL);
    deFree(rep);
    xcb_flush(connection);
}

XcbWindow::~XcbWindow(void)
{
    xcb_flush(m_display.getConnection());
    xcb_free_colormap(m_display.getConnection(), m_colormap);
    xcb_destroy_window(m_display.getConnection(), m_window);
}

void XcbWindow::setVisibility(bool visible)
{
    if (visible == m_visible)
        return;

    if (visible)
        xcb_map_window(m_display.getConnection(), m_window);
    else
        xcb_unmap_window(m_display.getConnection(), m_window);

    m_visible = visible;
    xcb_flush(m_display.getConnection());
}

void XcbWindow::processEvents(void)
{
    // A bit of a hack, since we don't really handle all the events.
    m_display.processEvents();
}

void XcbWindow::getDimensions(int *width, int *height) const
{
    xcb_get_geometry_reply_t *geom;
    geom =
        xcb_get_geometry_reply(m_display.getConnection(), xcb_get_geometry(m_display.getConnection(), m_window), NULL);
    *height = static_cast<int>(geom->height);
    *width  = static_cast<int>(geom->width);
    deFree(geom);
}

void XcbWindow::setDimensions(int width, int height)
{
    const uint32_t values[] = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    xcb_void_cookie_t result;
    xcb_connection_t *display = m_display.getConnection();
    result = xcb_configure_window(display, m_window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
    DE_ASSERT(nullptr == xcb_request_check(display, result));
    xcb_flush(display);

    for (;;)
    {
        xcb_generic_event_t *event = xcb_poll_for_event(display);
        int w, h;
        if (event != nullptr)
        {
            if (XCB_PROPERTY_NOTIFY == (event->response_type & ~0x80))
            {
                const xcb_property_notify_event_t *pnEvent = (xcb_property_notify_event_t *)event;
                if (pnEvent->atom == XCB_ATOM_RESOLUTION)
                {
                    deFree(event);
                    break;
                }
            }
            deFree(event);
        }
        getDimensions(&w, &h);
        if (h == height || w == width)
            break;
    }
}

} // namespace x11
} // namespace lnx
} // namespace tcu
