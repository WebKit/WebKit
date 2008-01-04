/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Trolltech ASA
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Frame.h"

#include "Element.h"
#include "RenderObject.h"
#include "RenderWidget.h"
#include "RenderLayer.h"
#include "Page.h"
#include "Document.h"
#include "HTMLElement.h"
#include "DOMWindow.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClientQt.h"
#include "DOMImplementation.h"
#include "ResourceHandleInternal.h"
#include "Document.h"
#include "Settings.h"
#include "Plugin.h"
#include "FrameView.h"
#include "FramePrivate.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "ResourceHandle.h"
#include "FrameLoader.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "SelectionController.h"
#include "kjs_proxy.h"
#include "TypingCommand.h"
#include "JSLock.h"
#include "kjs_window.h"
#include "runtime_root.h"
#include "runtime.h"
#include <QScrollArea>
#include "NotImplemented.h"

namespace WebCore {

// FIXME: Turned this off to fix buildbot. This function be either deleted or used.
#if 0
static void doScroll(const RenderObject* r, bool isHorizontal, int multiplier)
{
    // FIXME: The scrolling done here should be done in the default handlers
    // of the elements rather than here in the part.
    if (!r)
        return;

    //broken since it calls scroll on scrollbars
    //and we have none now
    //r->scroll(direction, KWQScrollWheel, multiplier);
    if (!r->layer())
        return;

    int x = r->layer()->scrollXOffset();
    int y = r->layer()->scrollYOffset();
    if (isHorizontal)
        x += multiplier;
    else
        y += multiplier;

    r->layer()->scrollToOffset(x, y, true, true);
}
#endif

KJS::Bindings::Instance* Frame::createScriptInstanceForWidget(WebCore::Widget* widget)
{
    QWidget* nativeWidget = widget->nativeWidget();
    if (!nativeWidget)
        return 0;
    return KJS::Bindings::Instance::createBindingForLanguageInstance(KJS::Bindings::Instance::QtLanguage,
                                                                     nativeWidget,
                                                                     bindingRootObject());
}

void Frame::clearPlatformScriptObjects()
{
}

DragImageRef Frame::dragImageForSelection() 
{
    return 0;
}
    
void Frame::dashboardRegionsChanged()
{
}

}
// vim: ts=4 sw=4 et
