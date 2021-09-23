/*
 * Copyright (C) 2011 Igalia S.L.
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

#include "config.h"
#include "WebDragClient.h"

#if ENABLE(DRAG_SUPPORT)

//#include "ArgumentCodersWPE.h"
#include "ShareableBitmap.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/DataTransfer.h>
#include <WebCore/DragData.h>
#include <WebCore/Pasteboard.h>
#include <wtf/win/GDIObject.h>
#include <WebCore/Frame.h>

//#include <WebCore/SelectionData.h>

namespace WebKit {
using namespace WebCore;

void WebDragClient::didConcludeEditDrag()
{
}

void WebDragClient::startDrag(DragItem, DataTransfer& dataTransfer, Frame& frame)
{
    m_page->willStartDrag();
    m_page->send(Messages::WebPageProxy::StartDrag(dataTransfer.pasteboard().createDragDataMap()));
}

}; // namespace WebKit.

#endif // ENABLE(DRAG_SUPPORT)
