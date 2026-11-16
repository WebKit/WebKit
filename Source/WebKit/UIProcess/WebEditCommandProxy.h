/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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

#pragma once

#include "APIObject.h"
#include "WebUndoStepID.h"
#include <WebCore/EditAction.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebPageProxy;
class WebProcessProxy;

class WebEditCommandProxy : public API::ObjectImpl<API::Object::Type::EditCommandProxy>, public CanMakeWeakPtr<WebEditCommandProxy> {
public:
    static Ref<WebEditCommandProxy> create(WebUndoStepID commandID, String&& label, WebPageProxy& page, WebProcessProxy& process, WebCore::PageIdentifier pageIDInProcess)
    {
        return adoptRef(*new WebEditCommandProxy(commandID, WTF::move(label), page, process, pageIDInProcess));
    }
    ~WebEditCommandProxy();

    WebUndoStepID commandID() const { return m_commandID; }
    String label() const { return m_label; }

    RefPtr<WebProcessProxy> process() const;
    WebCore::PageIdentifier pageIDInProcess() const { return m_pageIDInProcess; }

    void invalidate()
    {
        m_page.clear();
        m_process.clear();
    }

    void unapply();
    void reapply();

private:
    WebEditCommandProxy(WebUndoStepID commandID, String&& label, WebPageProxy&, WebProcessProxy&, WebCore::PageIdentifier pageIDInProcess);

    WebUndoStepID m_commandID;
    String m_label;
    WeakPtr<WebPageProxy> m_page;
    WeakPtr<WebProcessProxy> m_process;
    WebCore::PageIdentifier m_pageIDInProcess;
};

} // namespace WebKit
