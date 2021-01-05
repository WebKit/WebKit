/*
 * Copyright (C) 2014 Haiku, inc.
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
#include "WebInspectorProxy.h"

#if ENABLE(INSPECTOR)

#include "WebProcessProxy.h"
#include <WebCore/NotImplemented.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

void WebInspectorProxy::platformHide()
{
    notImplemented();
}

void WebInspectorProxy::platformResetState()
{
    notImplemented();
}

void WebInspectorProxy::platformBringToFront()
{
    notImplemented();
}

bool WebInspectorProxy::platformIsFront()
{
    notImplemented();
    return false;
}

void WebInspectorProxy::platformInspectedURLChanged(const String& url)
{
    notImplemented();
}

String WebInspectorProxy::inspectorPageURL()
{
    StringBuilder builder;
    builder.append(inspectorBaseURL());
    builder.appendLiteral("/Main.html");

    return builder.toString();
}

String WebInspectorProxy::inspectorTestPageURL()
{
    StringBuilder builder;
    builder.append(inspectorBaseURL());
    builder.appendLiteral("/Test.html");

    return builder.toString();
}

String WebInspectorProxy::inspectorBaseURL()
{
    notImplemented();
    return "file://" /*+ WebCore::inspectorResourcePath()*/;
}

unsigned WebInspectorProxy::platformInspectedWindowHeight()
{
    notImplemented();
    return 0;
}

unsigned WebInspectorProxy::platformInspectedWindowWidth()
{
    notImplemented();
    return 0;
}

void WebInspectorProxy::platformAttach()
{
    notImplemented();
}

void WebInspectorProxy::platformDetach()
{
    notImplemented();
}

void WebInspectorProxy::platformSetAttachedWindowHeight(unsigned)
{
    notImplemented();
}

void WebInspectorProxy::platformSetAttachedWindowWidth(unsigned)
{
    notImplemented();
}

void WebInspectorProxy::platformSave(const String&, const String&, bool, bool)
{
    notImplemented();
}

void WebInspectorProxy::platformAppend(const String&, const String&)
{
    notImplemented();
}

void WebInspectorProxy::platformAttachAvailabilityChanged(bool)
{
    notImplemented();
}

void WebInspectorProxy::platformStartWindowDrag()
{
	notImplemented();
}

void WebInspectorProxy::platformCreateFrontendWindow()
{
	notImplemented();
}
void WebInspectorProxy::platformCloseFrontendPageAndWindow()
{
	notImplemented();
}

void WebInspectorProxy::platformShowCertificate(const WebCore::CertificateInfo&)
{
	notImplemented();
}

void WebInspectorProxy::platformDidCloseForCrash()
{
	notImplemented();
}
void WebInspectorProxy::platformInvalidate()
{
	notImplemented();
}
void WebInspectorProxy::platformBringInspectedPageToFront()
{
	notImplemented();
}
WebPageProxy* WebInspectorProxy::platformCreateFrontendPage()
{
	notImplemented();
}

}

#endif
