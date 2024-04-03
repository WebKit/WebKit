/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(ATTACHMENT_ELEMENT)

#include "APIObject.h"
#include "WKBase.h"
#include <WebCore/AttachmentAssociatedElement.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSData;
OBJC_CLASS NSFileWrapper;
OBJC_CLASS NSString;

namespace WebCore {
enum class AttachmentAssociatedElementType : uint8_t;

class SharedBuffer;
class FragmentedSharedBuffer;
}

namespace WebKit {
class WebPageProxy;
}

namespace API {

class Attachment final : public ObjectImpl<Object::Type::Attachment> {
public:
    static Ref<Attachment> create(const WTF::String& identifier, WebKit::WebPageProxy&);
    virtual ~Attachment();

    enum class InsertionState : uint8_t { NotInserted, Inserted };

    const WTF::String& identifier() const { return m_identifier; }
    void updateAttributes(CompletionHandler<void()>&&);

    void invalidate();
    bool isValid() const { return !!m_webPage; }

#if PLATFORM(COCOA)
    void doWithFileWrapper(Function<void(NSFileWrapper *)>&&) const;
    void setFileWrapper(NSFileWrapper *);
    void setFileWrapperAndUpdateContentType(NSFileWrapper *, NSString *contentType);
    WTF::String utiType() const;
#endif
    WTF::String mimeType() const;

    const WTF::String& filePath() const { return m_filePath; }
    void setFilePath(const WTF::String& filePath) { m_filePath = filePath; }
    WTF::String fileName() const;

    const WTF::String& contentType() const { return m_contentType; }
    void setContentType(const WTF::String& contentType) { m_contentType = contentType; }

    InsertionState insertionState() const { return m_insertionState; }
    void setInsertionState(InsertionState state) { m_insertionState = state; }

    bool isEmpty() const;

    RefPtr<WebCore::FragmentedSharedBuffer> associatedElementData() const;
#if PLATFORM(COCOA)
    NSData *associatedElementNSData() const;
#endif
    std::optional<uint64_t> fileSizeForDisplay() const;

    void setAssociatedElementType(WebCore::AttachmentAssociatedElementType associatedElementType) { m_associatedElementType = associatedElementType; }
    WebCore::AttachmentAssociatedElementType associatedElementType() const { return m_associatedElementType; }

    RefPtr<WebCore::SharedBuffer> createSerializedRepresentation() const;
    void updateFromSerializedRepresentation(Ref<WebCore::SharedBuffer>&&, const WTF::String& contentType);

private:
    explicit Attachment(const WTF::String& identifier, WebKit::WebPageProxy&);

#if PLATFORM(COCOA)
    mutable Lock m_fileWrapperLock;
    RetainPtr<NSFileWrapper> m_fileWrapper WTF_GUARDED_BY_LOCK(m_fileWrapperLock);
#endif
    WTF::String m_identifier;
    WTF::String m_filePath;
    WTF::String m_contentType;
    WeakPtr<WebKit::WebPageProxy> m_webPage;
    InsertionState m_insertionState { InsertionState::NotInserted };
    WebCore::AttachmentAssociatedElementType m_associatedElementType { WebCore::AttachmentAssociatedElementType::None };
};

} // namespace API

#endif // ENABLE(ATTACHMENT_ELEMENT)
