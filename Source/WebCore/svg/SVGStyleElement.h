/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 */

#pragma once

#include "InlineStyleSheetOwner.h"
#include "SVGElement.h"

namespace WebCore {

class SVGStyleElement final : public SVGElement {
public:
    static Ref<SVGStyleElement> create(const QualifiedName&, Document&, bool createdByParser);
    virtual ~SVGStyleElement();

    CSSStyleSheet* sheet() const { return m_styleSheetOwner.sheet(); }

    bool disabled() const;
    void setDisabled(bool);
                          
    const AtomicString& type() const;
    void setType(const AtomicString&);

    const AtomicString& media() const;
    void setMedia(const AtomicString&);

private:
    SVGStyleElement(const QualifiedName&, Document&, bool createdByParser);

    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    InsertionNotificationRequest insertedInto(ContainerNode&) final;
    void removedFrom(ContainerNode&) final;
    void childrenChanged(const ChildChange&) final;

    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    void finishParsingChildren() final;

    virtual bool isLoading() const { return m_styleSheetOwner.isLoading(); }
    bool sheetLoaded() final { return m_styleSheetOwner.sheetLoaded(*this); }
    void startLoadingDynamicSheet() final { m_styleSheetOwner.startLoadingDynamicSheet(*this); }
    Timer* svgLoadEventTimer() final { return &m_svgLoadEventTimer; }

    String title() const final;

    InlineStyleSheetOwner m_styleSheetOwner;
    Timer m_svgLoadEventTimer;
};

} // namespace WebCore
