/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "FrameQt.h"

#include "DOMImplementation.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Document.h"
#include "EditorClientQt.h"
#include "Element.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FramePrivate.h"
#include "FrameLoaderClientQt.h"
#include "ResourceLoader.h"
#include "DocumentLoader.h"
#include "FrameView.h"
#include "FormState.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLFormElement.h"
#include "HistoryItem.h"
#include "JSLock.h"
#include "MouseEventWithHitTestResults.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "Plugin.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include "RenderWidget.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "SelectionController.h"
#include "Chrome.h"
#include "Settings.h"
#include "TypingCommand.h"
#include "kjs_window.h"
#include "runtime_root.h"

#include <QScrollArea>
#include <qdebug.h>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__); } while(0)

namespace WebCore {


Frame* FrameLoader::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer)
{
    notImplemented();
    return 0;
}

ObjectContentType FrameLoader::objectContentType(const KURL&, const String& mimeType)
{
    notImplemented();
    return ObjectContentType();
}

Widget* FrameLoader::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&)
{
    notImplemented();
    return 0;
}

Widget* FrameLoader::createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>&)
{
    notImplemented();
    return 0;
}

String FrameLoader::overrideMediaType() const
{
    // no-op
    return String();
}

void FrameLoader::partClearedInBegin()
{
    if (m_frame->settings()->isJavaScriptEnabled())
        static_cast<FrameLoaderClientQt*>(m_client)->partClearedInBegin();
}

void FrameLoader::didChangeTitle(DocumentLoader* loader)
{
    notImplemented();
    m_client->didChangeTitle(loader);
}

void FrameLoader::redirectDataToPlugin(Widget* pluginWidget)
{
    notImplemented();
}


}

// vim: ts=4 sw=4 et
