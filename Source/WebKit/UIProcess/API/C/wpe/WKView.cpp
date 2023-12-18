/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "WKView.h"

#include "APIClient.h"
#include "APIPageConfiguration.h"
#include "PageClientImpl.h"
#include "WKAPICast.h"
#include "WPEWebView.h"

using namespace WebKit;

#if ENABLE(WPE_PLATFORM)
WKViewRef WKViewCreate(WPEDisplay* display, WKPageConfigurationRef configuration)
{
    return toAPI(WKWPE::View::create(nullptr, display, *toImpl(configuration)));
}
#endif

WKViewRef WKViewCreateDeprecated(struct wpe_view_backend* backend, WKPageConfigurationRef configuration)
{
#if ENABLE(WPE_PLATFORM)
    return toAPI(WKWPE::View::create(backend, nullptr, *toImpl(configuration)));
#else
    return toAPI(WKWPE::View::create(backend, *toImpl(configuration)));
#endif
}

WKPageRef WKViewGetPage(WKViewRef view)
{
    return toAPI(&toImpl(view)->page());
}

#if ENABLE(WPE_PLATFORM)
WPEView* WKViewGetView(WKViewRef view)
{
    return toImpl(view)->wpeView();
}
#endif
