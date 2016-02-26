/*
 * Copyright (C) 2012 Samsung Electronics
 * Copyright (C) 2016 Naver Corp. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPopupMenuProxyEfl.h"

#include "EwkView.h"

namespace WebKit {

WebPopupMenuProxyEfl::WebPopupMenuProxyEfl(EwkView& ewkView, WebPopupMenuProxy::Client& client)
    : WebPopupMenuProxy(client)
    , m_ewkView(ewkView)
{
}

void WebPopupMenuProxyEfl::valueChanged(int newSelectedIndex)
{
    if (!m_client)
        return;

    m_client->valueChangedForPopupMenu(this, newSelectedIndex);
    invalidate();
}

void WebPopupMenuProxyEfl::showPopupMenu(const WebCore::IntRect& rect, WebCore::TextDirection textDirection, double pageScaleFactor, const Vector<WebPopupItem>& items, const PlatformPopupMenuData&, int32_t selectedIndex)
{
    m_ewkView.requestPopupMenu(this, rect, textDirection, pageScaleFactor, items, selectedIndex);
}

void WebPopupMenuProxyEfl::hidePopupMenu()
{
    m_ewkView.closePopupMenu();
}

} // namespace WebKit

