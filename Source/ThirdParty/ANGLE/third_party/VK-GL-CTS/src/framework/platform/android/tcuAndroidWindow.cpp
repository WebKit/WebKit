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
 * \brief Android window.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidWindow.hpp"

namespace tcu
{
namespace Android
{

using std::vector;

// Window

Window::Window(ANativeWindow *window) : m_window(window), m_state(STATE_AVAILABLE)
{
}

Window::~Window(void)
{
}

void Window::setBuffersGeometry(int width, int height, int32_t format)
{
    ANativeWindow_setBuffersGeometry(m_window, width, height, format);
}

IVec2 Window::getSize(void) const
{
    const int32_t width  = ANativeWindow_getWidth(m_window);
    const int32_t height = ANativeWindow_getHeight(m_window);
    return IVec2(width, height);
}

bool Window::tryAcquire(void)
{
    de::ScopedLock lock(m_stateLock);

    if (m_state == STATE_AVAILABLE)
    {
        m_state = STATE_IN_USE;
        return true;
    }
    else
        return false;
}

void Window::release(void)
{
    de::ScopedLock lock(m_stateLock);

    if (m_state == STATE_IN_USE)
    {
        // Reset buffer size and format back to initial state
        ANativeWindow_setBuffersGeometry(m_window, 0, 0, 0);

        m_state = STATE_AVAILABLE;
    }
    else if (m_state == STATE_PENDING_DESTROY)
        m_state = STATE_READY_FOR_DESTROY;
    else
        DE_FATAL("Invalid window state");
}

void Window::markForDestroy(void)
{
    de::ScopedLock lock(m_stateLock);

    if (m_state == STATE_AVAILABLE)
        m_state = STATE_READY_FOR_DESTROY;
    else if (m_state == STATE_IN_USE)
        m_state = STATE_PENDING_DESTROY;
    else
        DE_FATAL("Invalid window state");
}

bool Window::isPendingDestroy(void) const
{
    de::ScopedLock lock(m_stateLock);
    return m_state == STATE_PENDING_DESTROY;
}

bool Window::tryAcquireForDestroy(bool onlyMarked)
{
    de::ScopedLock lock(m_stateLock);

    if (m_state == STATE_READY_FOR_DESTROY || (!onlyMarked && m_state == STATE_AVAILABLE))
    {
        m_state = STATE_ACQUIRED_FOR_DESTROY;
        return true;
    }
    else
        return false;
}

// WindowRegistry

WindowRegistry::WindowRegistry(void)
{
}

WindowRegistry::~WindowRegistry(void)
{
    for (vector<Window *>::const_iterator winIter = m_windows.begin(); winIter != m_windows.end(); winIter++)
    {
        Window *const window = *winIter;

        if (window->tryAcquireForDestroy(false))
            delete window;
        else
        {
            print("ERROR: Window was not available for deletion, leaked tcu::Android::Window!\n");
            DE_FATAL("Window leaked");
        }
    }
}

void WindowRegistry::addWindow(ANativeWindow *window)
{
    m_windows.reserve(m_windows.size() + 1);
    m_windows.push_back(new Window(window));
}

void WindowRegistry::destroyWindow(ANativeWindow *rawHandle)
{
    for (int ndx = 0; ndx < (int)m_windows.size(); ++ndx)
    {
        Window *const window = m_windows[ndx];

        if (window->getNativeWindow() == rawHandle)
        {
            if (window->tryAcquireForDestroy(false))
            {
                delete window;
                m_windows[ndx] = m_windows.back();
                m_windows.pop_back();
            }
            else
                window->markForDestroy();

            return;
        }
    }

    throw tcu::InternalError("Window not registered", nullptr, __FILE__, __LINE__);
}

Window *WindowRegistry::tryAcquireWindow(void)
{
    for (int ndx = 0; ndx < (int)m_windows.size(); ++ndx)
    {
        Window *const window = m_windows[ndx];

        if (window->tryAcquire())
            return window;
    }

    return nullptr;
}

void WindowRegistry::garbageCollect(void)
{
    for (int ndx = 0; ndx < (int)m_windows.size(); ++ndx)
    {
        Window *const window = m_windows[ndx];

        if (window->tryAcquireForDestroy(true))
        {
            delete window;
            m_windows[ndx] = m_windows.back();
            m_windows.pop_back();
            ndx -= 1;
        }
    }
}

} // namespace Android
} // namespace tcu
