/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef GraphicsContext3DPrivate_h
#define GraphicsContext3DPrivate_h

#include <wtf/PassOwnPtr.h>

typedef struct __GLXcontextRec *GLXContext;
typedef unsigned long GLXPbuffer;
typedef unsigned long GLXPixmap;
typedef unsigned char GLubyte;
typedef unsigned long Pixmap;

namespace WebCore {

class GraphicsContext3D;

class GraphicsContext3DPrivate {
public:
    static PassOwnPtr<GraphicsContext3DPrivate> create();
    ~GraphicsContext3DPrivate();
    bool makeContextCurrent();

private:
    friend class GraphicsContext3D;
    static GraphicsContext3DPrivate* createPbufferContext();
    static GraphicsContext3DPrivate* createPixmapContext();
    GraphicsContext3DPrivate(GLXContext, GLXPbuffer);
    GraphicsContext3DPrivate(GLXContext, Pixmap, GLXPixmap);

    static void addActiveGraphicsContext(GraphicsContext3D*);
    static void removeActiveGraphicsContext(GraphicsContext3D*);
    static void cleanupActiveContextsAtExit();

    GLXContext m_context;
    GLXPbuffer m_pbuffer;
    Pixmap m_pixmap;
    GLXPixmap m_glxPixmap;
};

}

#endif // GraphicsContext3DPrivate_h
