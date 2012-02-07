/*
 * Copyright (C) 2012 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#ifndef WindowGLContext_h
#define WindowGLContext_h

#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

#if defined(XP_UNIX)
typedef struct __GLXcontextRec* GLXContext;
typedef struct _XDisplay Display;
#endif

namespace WebCore {

class WindowGLContext {
    WTF_MAKE_NONCOPYABLE(WindowGLContext);
public:
    static PassOwnPtr<WindowGLContext> createContextWithGdkWindow(GdkWindow*);
    virtual ~WindowGLContext();
    void startDrawing();
    void finishDrawing();

private:
    WindowGLContext(GdkWindow*);
    GdkWindow* m_window;

#if defined(XP_UNIX)
    GLXContext m_context;
    Display* m_display;
    bool m_needToCloseDisplay;
#endif
};

}

#endif // WindowGLContext_h
