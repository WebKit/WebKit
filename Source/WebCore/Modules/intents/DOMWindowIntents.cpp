/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "DOMWindowIntents.h"

#if ENABLE(WEB_INTENTS)

#include "DOMWindow.h"
#include "DeliveredIntent.h"

namespace WebCore {

DOMWindowIntents::DOMWindowIntents(DOMWindow* window)
    : DOMWindowProperty(window->frame())
{
}

DOMWindowIntents::~DOMWindowIntents()
{
}

DOMWindowIntents* DOMWindowIntents::from(DOMWindow* window)
{
    ASSERT(window);
    DEFINE_STATIC_LOCAL(AtomicString, name, ("DOMWindowIntents"));
    DOMWindowIntents* supplement = static_cast<DOMWindowIntents*>(Supplement<DOMWindow>::from(window, name));
    if (!supplement) {
        supplement = new DOMWindowIntents(window);
        provideTo(window, name, adoptPtr(supplement));
    }
    return supplement;
}

DeliveredIntent* DOMWindowIntents::webkitIntent(DOMWindow* window)
{
    return from(window)->webkitIntent();
}

DeliveredIntent* DOMWindowIntents::webkitIntent()
{
    return m_intent.get();
}

void DOMWindowIntents::deliver(PassRefPtr<DeliveredIntent> intent)
{
    if (!frame())
        return;

    m_intent = intent;
}

} // namespace WebCore

#endif // ENABLE(WEB_INTENTS)
