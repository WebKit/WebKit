/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <WebCore/EditorInsertAction.h>
#include <WebCore/TextAffinity.h>
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class DocumentFragment;
class Node;
class SharedBuffer;
class StyleProperties;
struct SimpleRange;
}

namespace WebKit {
class WebPage;
}

namespace API {

namespace InjectedBundle {

class EditorClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~EditorClient() { }

    virtual bool shouldBeginEditing(WebKit::WebPage&, const WebCore::SimpleRange&) { return true; }
    virtual bool shouldEndEditing(WebKit::WebPage&, const WebCore::SimpleRange&) { return true; }
    virtual bool shouldInsertNode(WebKit::WebPage&, WebCore::Node&, const Optional<WebCore::SimpleRange>&, WebCore::EditorInsertAction) { return true; }
    virtual bool shouldInsertText(WebKit::WebPage&, const WTF::String&, const Optional<WebCore::SimpleRange>&, WebCore::EditorInsertAction) { return true; }
    virtual bool shouldDeleteRange(WebKit::WebPage&, const Optional<WebCore::SimpleRange>&) { return true; }
    virtual bool shouldChangeSelectedRange(WebKit::WebPage&, const Optional<WebCore::SimpleRange>&, const Optional<WebCore::SimpleRange>&, WebCore::EAffinity, bool) { return true; }
    virtual bool shouldApplyStyle(WebKit::WebPage&, const WebCore::StyleProperties&, const Optional<WebCore::SimpleRange>&) { return true; }
    virtual void didBeginEditing(WebKit::WebPage&, const WTF::String&) { }
    virtual void didEndEditing(WebKit::WebPage&, const WTF::String&) { }
    virtual void didChange(WebKit::WebPage&, const WTF::String&) { }
    virtual void didChangeSelection(WebKit::WebPage&, const WTF::String&) { }
    virtual void willWriteToPasteboard(WebKit::WebPage&, const Optional<WebCore::SimpleRange>&) { }
    virtual void getPasteboardDataForRange(WebKit::WebPage&, const Optional<WebCore::SimpleRange>&, Vector<WTF::String>&, Vector<RefPtr<WebCore::SharedBuffer>>&) { }
    virtual void didWriteToPasteboard(WebKit::WebPage&) { }
    virtual bool performTwoStepDrop(WebKit::WebPage&, WebCore::DocumentFragment&, const WebCore::SimpleRange&, bool) { return false; }
};

} // namespace InjectedBundle

}
