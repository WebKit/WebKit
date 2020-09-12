/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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

#include "FrameLoaderTypes.h"
#include "HTMLFrameOwnerElement.h"

namespace JSC {
class CallFrame;
}

namespace WebCore {

class HTMLFrameElementBase : public HTMLFrameOwnerElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLFrameElementBase);
public:
    WEBCORE_EXPORT URL location() const;
    WEBCORE_EXPORT void setLocation(const String&);
    void setLocation(JSC::JSGlobalObject&, const String&);

    ScrollbarMode scrollingMode() const final;

    WEBCORE_EXPORT int width();
    WEBCORE_EXPORT int height();

protected:
    HTMLFrameElementBase(const QualifiedName&, Document&);

    bool canLoad() const;

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void didFinishInsertingNode() final;
    void didAttachRenderers() override;

    void openURL(LockHistory = LockHistory::Yes, LockBackForwardList = LockBackForwardList::Yes);

    AtomString frameURL() const { return m_frameURL; }
    void setFrameURL(const AtomString& frameURL) { m_frameURL = frameURL; }

private:
    bool canLoadScriptURL(const URL&) const final;

    bool canLoadURL(const String& relativeURL) const;
    bool canLoadURL(const URL&) const;

    bool canContainRangeEndPoint() const final { return false; }

    bool supportsFocus() const final;
    void setFocus(bool) final;
    
    bool isURLAttribute(const Attribute&) const final;
    bool isHTMLContentAttribute(const Attribute&) const final;

    AtomString m_frameURL;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLFrameElementBase)
    static bool isType(const WebCore::HTMLElement& element) { return is<WebCore::HTMLFrameElement>(element) || is<WebCore::HTMLIFrameElement>(element); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::HTMLElement>(node) && isType(downcast<WebCore::HTMLElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
