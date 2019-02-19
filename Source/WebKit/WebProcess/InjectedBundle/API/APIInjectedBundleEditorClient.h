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
class CSSStyleDeclaration;
class DocumentFragment;
class Node;
class Range;
class SharedBuffer;
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

    virtual bool shouldBeginEditing(WebKit::WebPage&, WebCore::Range*) { return true; }
    virtual bool shouldEndEditing(WebKit::WebPage&, WebCore::Range*) { return true; }
    virtual bool shouldInsertNode(WebKit::WebPage&, WebCore::Node*, WebCore::Range* rangeToReplace, WebCore::EditorInsertAction) { return true; }
    virtual bool shouldInsertText(WebKit::WebPage&, StringImpl*, WebCore::Range* rangeToReplace, WebCore::EditorInsertAction) { return true; }
    virtual bool shouldDeleteRange(WebKit::WebPage&, WebCore::Range*) { return true; }
    virtual bool shouldChangeSelectedRange(WebKit::WebPage&, WebCore::Range* fromRange, WebCore::Range* toRange, WebCore::EAffinity affinity, bool stillSelecting) { return true; }
    virtual bool shouldApplyStyle(WebKit::WebPage&, WebCore::CSSStyleDeclaration*, WebCore::Range*) { return true; }
    virtual void didBeginEditing(WebKit::WebPage&, StringImpl* notificationName) { }
    virtual void didEndEditing(WebKit::WebPage&, StringImpl* notificationName) { }
    virtual void didChange(WebKit::WebPage&, StringImpl* notificationName) { }
    virtual void didChangeSelection(WebKit::WebPage&, StringImpl* notificationName) { }
    virtual void willWriteToPasteboard(WebKit::WebPage&, WebCore::Range*) { }
    virtual void getPasteboardDataForRange(WebKit::WebPage&, WebCore::Range*, Vector<WTF::String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer>>& pasteboardData) { }
    virtual void didWriteToPasteboard(WebKit::WebPage&) { }
    virtual bool performTwoStepDrop(WebKit::WebPage&, WebCore::DocumentFragment&, WebCore::Range&, bool) { return false; }
};

} // namespace InjectedBundle

}
