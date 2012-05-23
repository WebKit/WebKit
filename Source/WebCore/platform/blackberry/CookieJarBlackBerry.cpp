/*
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
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
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 */

#include "config.h"
#include "CookieJar.h"

#include "Cookie.h"
#include "CookieManager.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoaderClientBlackBerry.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PageGroupLoadDeferrer.h"
#include "Settings.h"
#include <wtf/text/CString.h>

namespace WebCore {

String cookies(Document const* document, KURL const& url)
{
    Frame* frame = document->frame();
    Page* page = frame ? frame->page() : 0;

    if (!page)
        return String();

    if (!(frame && frame->loader() && frame->loader()->client()))
        return String();

    if (!static_cast<FrameLoaderClientBlackBerry*>(frame->loader()->client())->cookiesEnabled())
        return String();

    ASSERT(document && url == document->cookieURL());
    // 'HttpOnly' cookies should no be accessible from scripts, so we filter them out here
    return cookieManager().getCookie(url, NoHttpOnlyCookie);
}

void setCookies(Document* document, KURL const& url, String const& value)
{
    Frame* frame = document->frame();
    Page* page = frame ? frame->page() : 0;

    if (!page)
        return;

    if (!(frame && frame->loader() && frame->loader()->client()))
        return;

    if (!static_cast<FrameLoaderClientBlackBerry*>(frame->loader()->client())->cookiesEnabled())
        return;

    ASSERT(document && url == document->cookieURL());
    cookieManager().setCookies(url, value);
}

bool cookiesEnabled(Document const*)
{
    // FIXME. Currently cookie is enabled by default, no setting on property page.
    return true;
}

bool getRawCookies(const Document* document, const KURL& url, Vector<Cookie>& rawCookies)
{
    Vector<ParsedCookie*> result;
    cookieManager().getRawCookies(result, url, WithHttpOnlyCookies);
    for (size_t i = 0; i < result.size(); i++)
        result[i]->appendWebCoreCookie(rawCookies);
    return true;
}

void deleteCookie(const Document* document, const KURL& url, const String& cookieName)
{
    // Cookies are not bound to the document. Therefore, we don't need to pass
    // in the document object to find the targeted cookies in cookie manager.
    cookieManager().removeCookieWithName(url, cookieName);
}

String cookieRequestHeaderFieldValue(const Document* document, const KURL &url)
{
    ASSERT(document);

    if (!(document->frame() && document->frame()->loader() && document->frame()->loader()->client()))
        return String();

    if (!static_cast<FrameLoaderClientBlackBerry*>(document->frame()->loader()->client())->cookiesEnabled())
        return String();

    return cookieManager().getCookie(url, WithHttpOnlyCookies);
}

} // namespace WebCore
