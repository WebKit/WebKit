/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef HTMLScriptElement_h
#define HTMLScriptElement_h

#include "HTMLElement.h"
#include "ScriptElement.h"

namespace WebCore {

class HTMLScriptElement final : public HTMLElement, public ScriptElement {
public:
    static Ref<HTMLScriptElement> create(const QualifiedName&, Document&, bool wasInsertedByParser, bool alreadyStarted = false);

    String text() const { return scriptContent(); }
    void setText(const String&);

    URL src() const;

    void setAsync(bool);
    bool async() const;

    void setCrossOrigin(const AtomicString&);
    String crossOrigin() const;

private:
    HTMLScriptElement(const QualifiedName&, Document&, bool wasInsertedByParser, bool alreadyStarted);

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    InsertionNotificationRequest insertedInto(ContainerNode&) override;
    void finishedInsertingSubtree() override;
    void childrenChanged(const ChildChange&) override;

    bool isURLAttribute(const Attribute&) const override;

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    String sourceAttributeValue() const override;
    String charsetAttributeValue() const override;
    String typeAttributeValue() const override;
    String languageAttributeValue() const override;
    String forAttributeValue() const override;
    String eventAttributeValue() const override;
    bool asyncAttributeValue() const override;
    bool deferAttributeValue() const override;
    bool hasSourceAttribute() const override;

    void dispatchLoadEvent() override;

    Ref<Element> cloneElementWithoutAttributesAndChildren(Document&) override;
};

} //namespace

#endif
