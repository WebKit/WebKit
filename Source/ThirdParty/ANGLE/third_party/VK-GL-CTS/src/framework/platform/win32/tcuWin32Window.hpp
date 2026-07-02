#ifndef _TCUWIN32WINDOW_HPP
#define _TCUWIN32WINDOW_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2016 The Android Open Source Project
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
 * \brief Generic Win32 window class.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuVector.hpp"
#include "tcuWin32API.h"

namespace tcu
{
namespace win32
{

class Window
{
public:
    Window(HINSTANCE instance, int width, int height);
    ~Window(void);

    void setVisible(bool visible);
    bool setForeground(void);
    void setSize(int width, int height);
    void setMinimized(bool minimize);

    LRESULT windowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void processEvents(void);

    IVec2 getSize(void) const;
    HWND getHandle(void) const
    {
        return m_window;
    }
    HDC getDeviceContext(void) const
    {
        return GetDC(m_window);
    }

private:
    Window(const Window &);
    Window operator=(const Window &);

    HWND m_window;
};

} // namespace win32
} // namespace tcu

#endif // _TCUWIN32WINDOW_HPP
