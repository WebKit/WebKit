/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "NetscapePlugin.h"

#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

NPError NetscapePlugin::setDrawingModel(NPDrawingModel drawingModel)
{
    // The drawing model can only be set from NPP_New.
    if (!m_inNPPNew)
        return NPERR_GENERIC_ERROR;

    switch (drawingModel) {
#ifndef NP_NO_QUICKDRAW
        case NPDrawingModelQuickDraw:
#endif
        case NPDrawingModelCoreGraphics:
        case NPDrawingModelCoreAnimation:
            m_drawingModel = drawingModel;
            break;

        default:
            return NPERR_GENERIC_ERROR;
    }

    return NPERR_NO_ERROR;
}
    
NPError NetscapePlugin::setEventModel(NPEventModel eventModel)
{
    // The event model can only be set from NPP_New.
    if (!m_inNPPNew)
        return NPERR_GENERIC_ERROR;

    switch (eventModel) {
#ifndef NP_NO_CARBON
        case NPEventModelCarbon:
#endif
        case NPEventModelCocoa:
            m_eventModel = eventModel;
            break;

        default:
            return NPERR_GENERIC_ERROR;
    }
    
    return NPERR_NO_ERROR;
}

bool NetscapePlugin::platformPostInitialize()
{
    if (m_drawingModel == static_cast<NPDrawingModel>(-1)) {
#ifndef NP_NO_QUICKDRAW
        // Default to QuickDraw if the plugin did not specify a drawing model.
        m_drawingModel = NPDrawingModelQuickDraw;
#else
        // QuickDraw is not available, so we can't default to it. Instead, default to CoreGraphics.
        m_drawingModel = NPDrawingModelCoreGraphics;
#endif
    }
    
    if (m_eventModel == static_cast<NPEventModel>(-1)) {
        // If the plug-in did not specify a drawing model we default to Carbon when it is available.
#ifndef NP_NO_CARBON
        m_eventModel = NPEventModelCarbon;
#else
        m_eventModel = NPEventModelCocoa;
#endif // NP_NO_CARBON
    }

#if !defined(NP_NO_CARBON) && !defined(NP_NO_QUICKDRAW)
    // The CA drawing model does not work with the Carbon event model.
    if (m_drawingModel == NPDrawingModelCoreAnimation && m_eventModel == NPEventModelCarbon)
        return false;
    
    // The Cocoa event model does not work with the QuickDraw drawing model.
    if (m_eventModel == NPEventModelCocoa && m_drawingModel == NPDrawingModelQuickDraw)
        return false;
#endif
    
#ifndef NP_NO_QUICKDRAW
    // Right now we don't support the QuickDraw drawing model at all
    if (m_drawingModel == NPDrawingModelQuickDraw)
        return false;
#endif

    return true;
}

static inline NPCocoaEvent initializeEvent(NPCocoaEventType type)
{
    NPCocoaEvent event;
    
    event.type = type;
    event.version = 0;
    
    return event;
}

void NetscapePlugin::platformPaint(GraphicsContext* context, const IntRect& dirtyRect)
{
    switch (m_eventModel) {
        case NPEventModelCocoa: {
            NPCocoaEvent event = initializeEvent(NPCocoaEventDrawRect);

            event.data.draw.context = context->platformContext();
            event.data.draw.x = dirtyRect.x() - m_frameRect.x();
            event.data.draw.y = dirtyRect.y() - m_frameRect.y();
            event.data.draw.width = dirtyRect.width();
            event.data.draw.height = dirtyRect.height();
            
            m_pluginModule->pluginFuncs().event(&m_npp, &event);
            break;
        }
        
        default:
            ASSERT_NOT_REACHED();
    }
}

} // namespace WebKit
