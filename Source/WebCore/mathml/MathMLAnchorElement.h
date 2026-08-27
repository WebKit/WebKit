/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "MathMLElement.h"
#include "URLDecomposition.h"
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class DOMTokenList;

enum class ReferrerPolicy : uint8_t;
enum class Relation : uint8_t;

// TODO(tannal): We need to move all code for links from MathMLElement to MathMLAnchorElement
// Once the legacy href attribute on all MathML elements is removed
class MathMLAnchorElement final : public MathMLElement, public URLDecomposition {
    WTF_MAKE_TZONE_ALLOCATED(MathMLAnchorElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MathMLAnchorElement);

public:
    static Ref<MathMLAnchorElement> create(const QualifiedName&, Document&);
    virtual ~MathMLAnchorElement();

    WEBCORE_EXPORT URL href() const;

    URL hrefURL() const;
    AtomString target() const final;

    URL fullURL() const final { return href(); }
    void setFullURL(const URL&) final;

    void defaultEventHandler(Event&);

    bool hasRel(Relation) const;
    DOMTokenList& relList();

    String referrerPolicyForBindings() const;
    ReferrerPolicy referrerPolicy() const;

private:
    MathMLAnchorElement(const QualifiedName&, Document&);

    AtomString effectiveTarget() const;

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;

    OptionSet<Relation> m_linkRelations;
    const std::unique_ptr<DOMTokenList> m_relList;
};

} // namespace WebCore
