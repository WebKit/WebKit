/*
 * Copyright (C) 2019 Carlos Eduardo Ramalho <cadubentzen@gmail.com>.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "PictureInPictureWindow.h"

#if ENABLE(PICTURE_IN_PICTURE_API)

#include "EventNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PictureInPictureWindow);

Ref<PictureInPictureWindow> PictureInPictureWindow::create(Document& document)
{
    auto window = adoptRef(*new PictureInPictureWindow(document));
    window->suspendIfNeeded();
    return window;
}

PictureInPictureWindow::PictureInPictureWindow(Document& document)
    : ActiveDOMObject(document)
{
}

PictureInPictureWindow::~PictureInPictureWindow() = default;

void PictureInPictureWindow::setSize(const IntSize& size)
{
    if (width() == size.width() && height() == size.height())
        return;
    
    m_size = size;
    queueTaskToDispatchEvent(*this, TaskSource::MediaElement, Event::create(eventNames().resizeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void PictureInPictureWindow::close()
{
    m_size = { 0, 0 };
}

} // namespace WebCore

#endif // ENABLE(PICTURE_IN_PICTURE_API)
