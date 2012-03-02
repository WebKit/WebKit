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

#ifndef GLContext_h
#define GLContext_h

#include "GraphicsContext3D.h"
#include "Widget.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

#if defined(XP_UNIX)
typedef struct __GLXcontextRec* GLXContext;
typedef struct _XDisplay Display;
typedef struct __GLXcontextRec *GLXContext;
typedef unsigned long GLXPbuffer;
typedef unsigned long GLXPixmap;
typedef unsigned char GLubyte;
typedef unsigned long Pixmap;
typedef unsigned long XID;
typedef void* ContextKeyType;
#endif

namespace WebCore {

class GLContext {
    WTF_MAKE_NONCOPYABLE(GLContext);
public:
    static GLContext* createSharingContext(GLContext* shareContext);
    static GLContext* getContextForWidget(PlatformWidget);
    static GLContext* getCurrent();
    static void removeActiveContext(GLContext*);
    static void removeActiveContextForWidget(PlatformWidget);

    virtual ~GLContext();
    bool makeContextCurrent();
    void swapBuffers();
    bool canRenderToDefaultFramebuffer();

#if ENABLE(WEBGL)
    PlatformGraphicsContext3D platformContext();
#endif

private:
    static void addActiveContext(GLContext*);
    static void cleanupActiveContextsAtExit();

#if defined(XP_UNIX)
    GLContext(GLXContext);
    GLContext(GLXContext, Pixmap, GLXPixmap);
    static GLContext* createContext(XID, GLXContext sharingContext = 0);
    static GLContext* createWindowContext(XID window, GLXContext sharingContext);
    static GLContext* createPbufferContext(GLXContext sharingContext);
    static GLContext* createPixmapContext(GLXContext sharingContext);

    GLXContext m_context;
    Display* m_display;

    XID m_window;
    GLXPbuffer m_pbuffer;
    Pixmap m_pixmap;
    GLXPixmap m_glxPixmap;
#endif
};

}

#endif // GLContext_h
