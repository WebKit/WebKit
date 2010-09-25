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

#include "InjectedBundleHitTestResult.h"

#include "InjectedBundleNodeHandle.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/KURL.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<InjectedBundleHitTestResult> InjectedBundleHitTestResult::create(const WebCore::HitTestResult& hitTestResult)
{
    return adoptRef(new InjectedBundleHitTestResult(hitTestResult));
}

PassRefPtr<InjectedBundleNodeHandle> InjectedBundleHitTestResult::nodeHandle() const
{
    return InjectedBundleNodeHandle::getOrCreate(m_hitTestResult.innerNonSharedNode());
}

WebFrame* InjectedBundleHitTestResult::webFrame() const
{
    Node* node = m_hitTestResult.innerNonSharedNode();
    if (!node)
        return 0;

    Document* document = node->document();
    if (!document)
        return 0;

    Frame* frame = document->frame();
    if (!frame)
        return 0;

    return static_cast<WebFrameLoaderClient*>(frame->loader()->client())->webFrame();
}

const String& InjectedBundleHitTestResult::absoluteLinkURL() const
{
    return m_hitTestResult.absoluteLinkURL().string();
}

} // WebKit
