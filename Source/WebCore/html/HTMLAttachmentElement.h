/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "HTMLElement.h"

namespace WebCore {

class File;
class HTMLImageElement;
class RenderAttachment;
class SharedBuffer;

class HTMLAttachmentElement final : public HTMLElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLAttachmentElement);
public:
    static Ref<HTMLAttachmentElement> create(const QualifiedName&, Document&);
    static const String& getAttachmentIdentifier(HTMLImageElement&);
    static URL archiveResourceURL(const String&);

    WEBCORE_EXPORT URL blobURL() const;
    WEBCORE_EXPORT File* file() const;

    enum class UpdateDisplayAttributes { No, Yes };
    void setFile(RefPtr<File>&&, UpdateDisplayAttributes = UpdateDisplayAttributes::No);

    const String& uniqueIdentifier() const { return m_uniqueIdentifier; }
    void setUniqueIdentifier(const String& uniqueIdentifier) { m_uniqueIdentifier = uniqueIdentifier; }

    void copyNonAttributePropertiesFromElement(const Element&) final;

    WEBCORE_EXPORT void updateAttributes(Optional<uint64_t>&& newFileSize, const String& newContentType, const String& newFilename);
    WEBCORE_EXPORT void updateEnclosingImageWithData(const String& contentType, Ref<SharedBuffer>&& data);

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;

    const String& ensureUniqueIdentifier();
    bool hasEnclosingImage() const;

    WEBCORE_EXPORT String attachmentTitle() const;
    String attachmentTitleForDisplay() const;
    String attachmentType() const;
    String attachmentPath() const;

    RenderAttachment* renderer() const;

private:
    HTMLAttachmentElement(const QualifiedName&, Document&);
    virtual ~HTMLAttachmentElement();

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool shouldSelectOnMouseDown() final {
#if PLATFORM(IOS_FAMILY)
        return false;
#else
        return true;
#endif
    }
    bool canContainRangeEndPoint() const final { return false; }
    void parseAttribute(const QualifiedName&, const AtomString&) final;
    
    RefPtr<File> m_file;
    String m_uniqueIdentifier;
};

} // namespace WebCore

#endif // ENABLE(ATTACHMENT_ELEMENT)
