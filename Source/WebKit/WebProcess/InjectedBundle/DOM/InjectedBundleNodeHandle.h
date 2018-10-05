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

#pragma once

#include "APIObject.h"
#include "ImageOptions.h"
#include <JavaScriptCore/JSBase.h>
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class IntRect;
class Node;
enum class AutoFillButtonType : uint8_t;
}

namespace WebKit {

class InjectedBundleRangeHandle;
class InjectedBundleScriptWorld;
class WebFrame;
class WebImage;

class InjectedBundleNodeHandle : public API::ObjectImpl<API::Object::Type::BundleNodeHandle> {
public:
    static RefPtr<InjectedBundleNodeHandle> getOrCreate(JSContextRef, JSObjectRef);
    static RefPtr<InjectedBundleNodeHandle> getOrCreate(WebCore::Node*);
    static Ref<InjectedBundleNodeHandle> getOrCreate(WebCore::Node&);

    virtual ~InjectedBundleNodeHandle();

    WebCore::Node* coreNode();

    // Convenience DOM Operations
    Ref<InjectedBundleNodeHandle> document();

    // Additional DOM Operations
    // Note: These should only be operations that are not exposed to JavaScript.
    WebCore::IntRect elementBounds();
    WebCore::IntRect renderRect(bool*);
    RefPtr<WebImage> renderedImage(SnapshotOptions, bool shouldExcludeOverflow, const std::optional<float>& bitmapWidth = std::nullopt);
    RefPtr<InjectedBundleRangeHandle> visibleRange();
    void setHTMLInputElementValueForUser(const String&);
    void setHTMLInputElementSpellcheckEnabled(bool);
    bool isHTMLInputElementAutoFilled() const;
    void setHTMLInputElementAutoFilled(bool);
    bool isHTMLInputElementAutoFillButtonEnabled() const;
    void setHTMLInputElementAutoFillButtonEnabled(WebCore::AutoFillButtonType);
    WebCore::AutoFillButtonType htmlInputElementAutoFillButtonType() const;
    WebCore::AutoFillButtonType htmlInputElementLastAutoFillButtonType() const;
    bool isAutoFillAvailable() const;
    void setAutoFillAvailable(bool);
    WebCore::IntRect htmlInputElementAutoFillButtonBounds();
    bool htmlInputElementLastChangeWasUserEdit();
    bool htmlTextAreaElementLastChangeWasUserEdit();
    bool isTextField() const;
    bool isSelectElement() const;
    
    RefPtr<InjectedBundleNodeHandle> htmlTableCellElementCellAbove();

    RefPtr<WebFrame> documentFrame();
    RefPtr<WebFrame> htmlFrameElementContentFrame();
    RefPtr<WebFrame> htmlIFrameElementContentFrame();

private:
    static Ref<InjectedBundleNodeHandle> create(WebCore::Node&);
    InjectedBundleNodeHandle(WebCore::Node&);

    Ref<WebCore::Node> m_node;
};

} // namespace WebKit
