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

#include "config.h"
#include "WKOpenPanelResultListener.h"

#include "APIArray.h"
#include "APIData.h"
#include "APIString.h"
#include "WKAPICast.h"
#include "WebOpenPanelResultListenerProxy.h"
#include <wtf/URL.h>

using namespace WebKit;

WKTypeID WKOpenPanelResultListenerGetTypeID()
{
    return toAPI(WebOpenPanelResultListenerProxy::APIType);
}

static Vector<String> filePathsFromFileURLs(const API::Array& fileURLs)
{
    Vector<String> filePaths;

    size_t size = fileURLs.size();
    filePaths.reserveInitialCapacity(size);

    for (size_t i = 0; i < size; ++i) {
        API::URL* apiURL = fileURLs.at<API::URL>(i);
        if (apiURL)
            filePaths.uncheckedAppend(URL(URL(), apiURL->string()).fileSystemPath());
    }

    return filePaths;
}

#if PLATFORM(IOS_FAMILY)
void WKOpenPanelResultListenerChooseMediaFiles(WKOpenPanelResultListenerRef listenerRef, WKArrayRef fileURLsRef, WKStringRef displayString, WKDataRef iconImageDataRef)
{
    toImpl(listenerRef)->chooseFiles(filePathsFromFileURLs(*toImpl(fileURLsRef)), toImpl(displayString)->string(), toImpl(iconImageDataRef));
}
#endif

void WKOpenPanelResultListenerChooseFiles(WKOpenPanelResultListenerRef listenerRef, WKArrayRef fileURLsRef)
{
    toImpl(listenerRef)->chooseFiles(filePathsFromFileURLs(*toImpl(fileURLsRef)));
}

void WKOpenPanelResultListenerCancel(WKOpenPanelResultListenerRef listenerRef)
{
    toImpl(listenerRef)->cancel();
}
