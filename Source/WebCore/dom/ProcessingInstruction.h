/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <WebCore/CachedResourceHandle.h>
#include <WebCore/CachedStyleSheetClient.h>
#include <WebCore/CharacterData.h>

namespace WebCore {

class StyleSheet;
class CSSStyleSheet;

class ProcessingInstruction final : public CharacterData, private CachedStyleSheetClient {
    WTF_MAKE_TZONE_ALLOCATED(ProcessingInstruction);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ProcessingInstruction);
public:
    USING_CAN_MAKE_WEAKPTR(CharacterData);

    static ExceptionOr<Ref<ProcessingInstruction>> create(Document&, RefPtr<AtomStringImpl> target, RefPtr<StringImpl> data);
    static Ref<ProcessingInstruction> createWithoutValidation(Document&, RefPtr<AtomStringImpl> target, RefPtr<StringImpl> data);
    virtual ~ProcessingInstruction();

    // CachedResourceClient.
    void ref() const final { CharacterData::ref(); }
    void deref() const final { CharacterData::deref(); }

    const RefPtr<StringImpl>& target() const LIFETIME_BOUND { return m_target; }

    void setCreatedByParser(bool createdByParser) { m_createdByParser = createdByParser; }

    // https://dom.spec.whatwg.org/#processinginstruction and the pseudo attribute
    // grammar that https://www.w3.org/TR/xml-stylesheet/ defines.
    bool hasPseudoAttributes();
    Vector<RefPtr<AtomStringImpl>> getAttributeNames();
    RefPtr<StringImpl> getAttribute(AtomStringImpl*);
    ExceptionOr<void> setAttribute(AtomStringImpl*, AtomStringImpl*);
    void removeAttribute(AtomStringImpl*);
    ExceptionOr<bool> toggleAttribute(AtomStringImpl*, std::optional<bool> force);
    bool hasAttribute(AtomStringImpl*);

    const RefPtr<StringImpl>& localHref() const LIFETIME_BOUND { return m_localHref; }
    StyleSheet* sheet() const { return m_sheet.get(); }

    bool isCSS() const { return m_isCSS; }
#if ENABLE(XSLT)
    bool isXSL() const { return m_isXSL; }
#endif

private:
    friend class CharacterData;
    ProcessingInstruction(Document&, RefPtr<AtomStringImpl> target, RefPtr<StringImpl> data);

    RefPtr<StringImpl> nodeName() const override;
    Ref<Node> cloneNodeInternal(Document&, CloningOperation, CustomElementRegistry*) const override;
    SerializedNode serializeNode(CloningOperation) const override;

    NeedsPostConnectionSteps insertionSteps(InsertionType, ContainerNode&) override;
    void postConnectionSteps() override;
    void removingSteps(RemovalType, ContainerNode&) override;

    void checkStyleSheet();
    void setSheet(RefPtr<StyleSheet>&&);
    void setCSSStyleSheet(StringImpl* href, const URL& baseURL, ASCIILiteral charset, const CachedCSSStyleSheet&) override;
#if ENABLE(XSLT)
    void setXSLStyleSheet(StringImpl* href, const URL& baseURL, StringImpl* sheet) override;
#endif

    bool isLoading() const;
    bool sheetLoaded() override;

    void addSubresourceAttributeURLs(OrderedHashSet<URL>&) const override;

    void parseStyleSheet(StringImpl* sheet);

    struct PseudoAttribute {
        RefPtr<AtomStringImpl> name;
        RefPtr<AtomStringImpl> value;
    };
    static std::optional<Vector<PseudoAttribute>> parsePseudoAttributes(StringView);

    void setDataAndUpdate(StringImpl*, unsigned offsetOfReplacedData, unsigned oldLength, unsigned newLength, UpdateLiveRanges) final;
    void updateAttributesIfNeeded();
    void updateDataFromAttributes();

    bool m_updatingDataFromAttributes { false };

    RefPtr<StringImpl> m_target;
    RefPtr<StringImpl> m_localHref;
    RefPtr<StringImpl> m_title;
    RefPtr<StringImpl> m_media;
    Vector<PseudoAttribute> m_attributes;
    bool m_attributesDirty { true };
    CachedResourceHandle<CachedResource> m_cachedSheet;
    RefPtr<StyleSheet> m_sheet;
    bool m_loading { false };
    bool m_alternate { false };
    bool m_createdByParser { false };
    bool m_isCSS { false };
#if ENABLE(XSLT)
    bool m_isXSL { false };
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ProcessingInstruction)
    static bool isType(const WebCore::Node& node) { return node.nodeType() == WebCore::NodeType::ProcessingInstruction; }
SPECIALIZE_TYPE_TRAITS_END()
