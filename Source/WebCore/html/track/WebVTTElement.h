/*
 * Copyright (C) 2013-2023 Apple Inc.  All rights reserved.
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

#if ENABLE(VIDEO)

#include "HTMLElement.h"
#include "RubyElement.h"
#include "RubyTextElement.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

enum WebVTTNodeType {
    WebVTTNodeTypeNone = 0,
    WebVTTNodeTypeClass,
    WebVTTNodeTypeItalic,
    WebVTTNodeTypeLanguage,
    WebVTTNodeTypeBold,
    WebVTTNodeTypeUnderline,
    WebVTTNodeTypeRuby,
    WebVTTNodeTypeRubyText,
    WebVTTNodeTypeVoice
};

class WebVTTElement;

class WebVTTElementImpl {
public:
    static Ref<Element> create(const WebVTTNodeType, AtomString language, Document&);
    Ref<HTMLElement> createEquivalentHTMLElement(Document&);

    Ref<Element> cloneElementWithoutAttributesAndChildren(Document&);

    void setWebVTTNodeType(WebVTTNodeType type) { m_webVTTNodeType = static_cast<unsigned>(type); }
    WebVTTNodeType webVTTNodeType() const { return static_cast<WebVTTNodeType>(m_webVTTNodeType); }

    bool isPastNode() const { return m_isPastNode; }
    void setIsPastNode(bool value) { m_isPastNode = value; }

    AtomString language() const { return m_language; }
    void setLanguage(const AtomString& value) { m_language = value; }

    static const QualifiedName& voiceAttributeName()
    {
        static NeverDestroyed<QualifiedName> voiceAttr(nullAtom(), "voice"_s, nullAtom());
        return voiceAttr;
    }

    static const QualifiedName& langAttributeName()
    {
        static NeverDestroyed<QualifiedName> voiceAttr(nullAtom(), "lang"_s, nullAtom());
        return voiceAttr;
    }

protected:
    WebVTTElementImpl(WebVTTNodeType nodeType, AtomString language)
        : m_isPastNode { false }
        , m_webVTTNodeType { static_cast<unsigned>(nodeType) }
        , m_language { language }
    {
    }
    virtual ~WebVTTElementImpl() = default;
    virtual Element& toElement() = 0;

    unsigned m_isPastNode : 1;
    unsigned m_webVTTNodeType : 4;

    AtomString m_language;
};

class WebVTTElement final : public WebVTTElementImpl, public Element {
    WTF_MAKE_ISO_ALLOCATED(WebVTTElement);
public:
    Ref<Element> cloneElementWithoutAttributesAndChildren(Document& document) final { return WebVTTElementImpl::cloneElementWithoutAttributesAndChildren(document); }

private:
    friend class WebVTTElementImpl;
    WebVTTElement(WebVTTNodeType, AtomString language, Document&);

    bool isWebVTTElement() const final { return true; }
    Element& toElement() final { return *this; }
};

class WebVTTRubyElement final : public WebVTTElementImpl, public RubyElement {
    WTF_MAKE_ISO_ALLOCATED(WebVTTRubyElement);
public:
    Ref<Element> cloneElementWithoutAttributesAndChildren(Document& document) final { return WebVTTElementImpl::cloneElementWithoutAttributesAndChildren(document); }

private:
    friend class WebVTTElementImpl;
    WebVTTRubyElement(AtomString language, Document& document)
        : WebVTTElementImpl(WebVTTNodeTypeRuby, language)
        , RubyElement(HTMLNames::rubyTag, document)
    {
    }

    bool isWebVTTRubyElement() const final { return true; }
    Element& toElement() final { return *this; }
};

class WebVTTRubyTextElement final : public WebVTTElementImpl, public RubyTextElement {
    WTF_MAKE_ISO_ALLOCATED(WebVTTRubyTextElement);
public:
    Ref<Element> cloneElementWithoutAttributesAndChildren(Document& document) final { return WebVTTElementImpl::cloneElementWithoutAttributesAndChildren(document); }

private:
    friend class WebVTTElementImpl;
    WebVTTRubyTextElement(AtomString language, Document& document)
        : WebVTTElementImpl(WebVTTNodeTypeRubyText, language)
        , RubyTextElement(HTMLNames::rtTag, document)
    {
    }

    bool isWebVTTRubyTextElement() const final { return true; }
    Element& toElement() final { return *this; }
};


} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebVTTElement)
    static bool isType(const WebCore::Node& node) { return node.isWebVTTElement(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebVTTRubyElement)
    static bool isType(const WebCore::Node& node) { return node.isWebVTTRubyElement(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebVTTRubyTextElement)
    static bool isType(const WebCore::Node& node) { return node.isWebVTTRubyTextElement(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO)
