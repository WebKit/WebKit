/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "NavigatorGeolocation.h"

#include "Geolocation.h"
#include "Navigator.h"

namespace WebCore {

NavigatorGeolocation::NavigatorGeolocation(Frame* frame)
    : DOMWindowProperty(frame)
{
}

NavigatorGeolocation::~NavigatorGeolocation()
{
}

void NavigatorGeolocation::willDetachPage()
{
    // FIXME: We should ideally allow existing Geolocation activities to continue
    // when the Geolocation's iframe is reparented. (Assuming we continue to
    // support reparenting iframes.)
    // See https://bugs.webkit.org/show_bug.cgi?id=55577
    // and https://bugs.webkit.org/show_bug.cgi?id=52877
    if (m_geolocation)
        m_geolocation->reset();
}

NavigatorGeolocation* NavigatorGeolocation::from(Navigator* navigator)
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("NavigatorGeolocation"));
    NavigatorGeolocation* supplement = static_cast<NavigatorGeolocation*>(NavigatorSupplement::from(navigator, name));
    if (!supplement) {
        supplement = new NavigatorGeolocation(navigator->frame());
        provideTo(navigator, name, adoptPtr(supplement));
    }
    return supplement;
}

Geolocation* NavigatorGeolocation::geolocation(Navigator* navigator)
{
    return NavigatorGeolocation::from(navigator)->geolocation();
}

Geolocation* NavigatorGeolocation::geolocation() const
{
    if (!m_geolocation)
        m_geolocation = Geolocation::create(frame());
    return m_geolocation.get();
}

} // namespace WebCore
