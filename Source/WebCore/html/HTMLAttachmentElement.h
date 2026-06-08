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

#include <wtf/Platform.h>
#if ENABLE(ATTACHMENT_ELEMENT)

#include <WebCore/HTMLElement.h>
#include <WebCore/Image.h>

namespace WebCore {

enum class AttachmentAssociatedElementType : uint8_t;

class AttachmentAssociatedElement;
class DOMRectReadOnly;
class File;
class HTMLImageElement;
class RenderAttachment;
class ShadowRoot;
class FragmentedSharedBuffer;

class HTMLAttachmentElement final : public HTMLElement {
    WTF_MAKE_TZONE_ALLOCATED(HTMLAttachmentElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(HTMLAttachmentElement);
public:
    static Ref<HTMLAttachmentElement> create(const QualifiedName&, Document&);
    WEBCORE_EXPORT static String getAttachmentIdentifier(HTMLElement&);
    static URL archiveResourceURL(const String&);

    WEBCORE_EXPORT URL blobURL() const;
    WEBCORE_EXPORT File* NODELETE file() const;

    enum class UpdateDisplayAttributes : bool { No, Yes };
    void setFile(RefPtr<File>&&, UpdateDisplayAttributes = UpdateDisplayAttributes::No);

    const String& uniqueIdentifier() const LIFETIME_BOUND { return m_uniqueIdentifier; }
    void setUniqueIdentifier(const String&);

    void copyNonAttributePropertiesFromElement(const Element&) final;

    WEBCORE_EXPORT void updateAttributes(std::optional<uint64_t>&& newFileSize, const AtomString& newContentType, const AtomString& newFilename);
    WEBCORE_EXPORT void updateAssociatedElementWithData(const String& contentType, Ref<FragmentedSharedBuffer>&& data);
    WEBCORE_EXPORT void updateIconForNarrowLayout(const RefPtr<Image>& icon, const WebCore::FloatSize&);
    WEBCORE_EXPORT void updateIconForWideLayout(Vector<uint8_t>&&);

    NeedsPostConnectionSteps insertionSteps(InsertionType, ContainerNode&) final;
    void removingSteps(RemovalType, ContainerNode&) final;

    String ensureUniqueIdentifier();
    AttachmentAssociatedElement* NODELETE associatedElement() const;
    AttachmentAssociatedElementType associatedElementType() const;

    WEBCORE_EXPORT String NODELETE attachmentTitle() const;
    const AtomString& NODELETE attachmentSubtitle() const;
    const AtomString& NODELETE attachmentActionForDisplay() const;
    String attachmentTitleForDisplay() const;
    const AtomString& NODELETE attachmentSubtitleForDisplay() const;
    WEBCORE_EXPORT String NODELETE attachmentType() const;
    String NODELETE attachmentPath() const;
    RefPtr<Image> icon() const { return m_icon; }
    void requestIconIfNeededWithSize(const FloatSize&);
    void requestWideLayoutIconIfNeeded();
    FloatSize iconSize() const { return m_iconSize; }
    void invalidateRendering();
    DOMRectReadOnly* saveButtonClientRect() const;

#if ENABLE(SERVICE_CONTROLS)
    bool isImageMenuEnabled() const { return m_isImageMenuEnabled; }
    void setImageMenuEnabled(bool value) { m_isImageMenuEnabled = value; }
#endif

    bool isWideLayout() const { return m_implementation == Implementation::WideLayout; }
    HTMLElement* wideLayoutShadowContainer() const { return m_containerElement.get(); }
    HTMLElement* NODELETE wideLayoutImageElement() const;
    WEBCORE_EXPORT static String shadowUserAgentStyleSheetText();

    enum class HighlightState : uint8_t {
        None, // The object is not selected.
        Start, // The object either contains the start of a selection run or is the start of a run
        Inside, // The object is fully encompassed by a selection run
        End, // The object either contains the end of a selection run or is the end of a run
        Both // The object contains an entire run or is the sole selected object in that run
    };
    void addSelectionClasses(HighlightState);

private:
    friend class AttachmentSaveEventListener;

    HTMLAttachmentElement(const QualifiedName&, Document&);
    virtual ~HTMLAttachmentElement();

    void didAddUserAgentShadowRoot(ShadowRoot&) final;
    void ensureWideLayoutShadowTree(ShadowRoot&);
    void updateProgress(const AtomString&);
    void updateSaveButton(bool);
    void updateImage();

    void NODELETE setNeedsIconRequest();

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool isReplaced(const RenderStyle* = nullptr) const final { return true; }
    bool shouldSelectOnMouseDown() final {
#if PLATFORM(IOS_FAMILY)
        return false;
#else
        return true;
#endif
    }
    bool canContainRangeEndPoint() const final { return false; }
    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;

#if ENABLE(SERVICE_CONTROLS)
    bool childShouldCreateRenderer(const Node&) const final;
#endif

    enum class Implementation: uint8_t { NarrowLayout, WideLayout };
    Implementation m_implementation { Implementation::NarrowLayout };

    RefPtr<File> m_file;
    String m_uniqueIdentifier;
    RefPtr<Image> m_icon;
    FloatSize m_iconSize;

    Vector<uint8_t> m_iconForWideLayout;

    const RefPtr<HTMLImageElement> m_imageElement;
    const RefPtr<HTMLElement> m_containerElement;
    const RefPtr<HTMLElement> m_placeholderElement;
    const RefPtr<HTMLElement> m_progressElement;
    const RefPtr<HTMLElement> m_informationBlock;
    const RefPtr<HTMLElement> m_actionTextElement;
    const RefPtr<HTMLElement> m_titleElement;
    const RefPtr<HTMLElement> m_subtitleElement;
    RefPtr<HTMLElement> m_saveArea;
    RefPtr<HTMLElement> m_saveButton;
    mutable RefPtr<DOMRectReadOnly> m_saveButtonClientRect;

    bool m_needsIconRequest { true };

#if ENABLE(SERVICE_CONTROLS)
    bool m_isImageMenuEnabled { false };
#endif
};

} // namespace WebCore

#endif // ENABLE(ATTACHMENT_ELEMENT)
