/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "LoadedWebArchive.h"
#include "MessageReceiver.h"
#include "WebBackForwardListItem.h"
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <wtf/Ref.h>
#include <wtf/ThreadGroup.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace API {
class Array;
}

namespace WebKit {

class FrameState;
class WebPageProxy;

struct BackForwardListState;
struct WebBackForwardListCounts;

enum class AllowSkippingBackForwardItems : bool { No, Yes };


// Avoid including WebKit-Swift.h in header files to avoid dependency loops.
class WebBackForwardList;
class WebBackForwardListMessageForwarder;

// This C++ stub object exists to forward API calls through to the Swift implementation.
// Although the BackForwardList is in Swift, we retain a C++
// API::Object subclass because Swift can't yet inherit from C++ -
// rdar://163102366
class WebBackForwardListWrapper : public API::ObjectImpl<API::Object::Type::BackForwardList> {
public:
    static Ref<WebBackForwardListWrapper> create(WebPageProxy& webPageProxy)
    {
        return adoptRef(*new WebBackForwardListWrapper(webPageProxy));
    }

    virtual ~WebBackForwardListWrapper();

    void removeAllItems();
    void clear();

    WebBackForwardListItem* WTF_NULLABLE currentItem() const;

    RefPtr<WebBackForwardListItem> itemAtDeltaFromCurrentIndex(int, AllowSkippingBackForwardItems = AllowSkippingBackForwardItems::Yes) const;
    RefPtr<WebBackForwardListItem> backItem() const;
    RefPtr<WebBackForwardListItem> forwardItem() const;

    Ref<API::Array> backList() const;
    Ref<API::Array> forwardList() const;

    unsigned backListCountForAPI() const;
    unsigned forwardListCountForAPI() const;

    Ref<API::Array> backListAsAPIArrayWithLimit(unsigned limit) const;
    Ref<API::Array> forwardListAsAPIArrayWithLimit(unsigned limit) const;

    String loggingString();

    WebBackForwardList& getImpl() { return *m_impl; }
    WebBackForwardListMessageForwarder& messageReceiver() const;

private:
    explicit WebBackForwardListWrapper(WebPageProxy&);

    std::unique_ptr<WebBackForwardList> m_impl;
    Ref<WebBackForwardListMessageForwarder> m_messageForwarder;
};


} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebBackForwardListWrapper)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::BackForwardList; }
SPECIALIZE_TYPE_TRAITS_END()
