#ifndef _TCULNXX11XCB_HPP
#define _TCULNXX11XCB_HPP
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

#include "tcuLnxX11.hpp"
#include <xcb/xcb.h>

namespace tcu
{
namespace lnx
{
namespace x11
{

class XcbDisplay : public DisplayBase
{
public:
    XcbDisplay(EventState &platform, const char *name);
    virtual ~XcbDisplay(void);

    xcb_screen_t *getScreen(void)
    {
        return m_screen;
    }
    xcb_connection_t *getConnection(void)
    {
        return m_connection;
    }

    void processEvents(void);
    static bool hasDisplay(const char *name);

    static DisplayState s_displayState;

protected:
    xcb_screen_t *m_screen;
    xcb_connection_t *m_connection;

private:
    XcbDisplay(const XcbDisplay &);
    XcbDisplay &operator=(const XcbDisplay &);
};

class XcbWindow : public WindowBase
{
public:
    XcbWindow(XcbDisplay &display, int width, int height, xcb_visualid_t *visual);
    ~XcbWindow(void);

    void setVisibility(bool visible);

    void processEvents(void);
    DisplayBase &getDisplay(void)
    {
        return (DisplayBase &)m_display;
    }
    xcb_window_t &getXID(void)
    {
        return m_window;
    }

    void getDimensions(int *width, int *height) const;
    void setDimensions(int width, int height);

protected:
    XcbDisplay &m_display;
    xcb_colormap_t m_colormap;
    xcb_window_t m_window;

private:
    XcbWindow(const XcbWindow &);
    XcbWindow &operator=(const XcbWindow &);
};

} // namespace x11
} // namespace lnx
} // namespace tcu

#endif // _TCULNXX11XCB_HPP
