/*
    Copyright (C) 2012 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef AcceleratedCompositingContext_h
#define AcceleratedCompositingContext_h

#include <config.h>
#include "TextureMapperFPSCounter.h"
#include "Timer.h"
#include <wtf/Noncopyable.h>

#include <Rect.h>

class BBitmap;
class BView;
class BWebView;

namespace WebCore {

class GraphicsLayer;
class HostWindow;
class TextureMapper;
class TextureMapperLayer;

class AcceleratedCompositingContext {
    WTF_MAKE_NONCOPYABLE(AcceleratedCompositingContext);
public:
    AcceleratedCompositingContext(BWebView* view);
    ~AcceleratedCompositingContext();

    void setRootGraphicsLayer(GraphicsLayer* rootLayer);
    bool isValid() { return m_rootLayer != NULL; }
    void flushAndRenderLayers();

private:
    void paintToGraphicsContext();
    void compositeLayers(BRect updateRect);

    bool flushPendingLayerChanges();
    void syncLayers();

    BWebView* m_view;
    BRect m_updateRect;

    GraphicsLayer* m_rootLayer;
    Timer m_syncTimer;

#if USE(TEXTURE_MAPPER)
    std::unique_ptr<TextureMapper> m_textureMapper;
    TextureMapperFPSCounter m_fpsCounter;
#endif
};

} // namespace WebCore

#endif // AcceleratedCompositingContext_h
