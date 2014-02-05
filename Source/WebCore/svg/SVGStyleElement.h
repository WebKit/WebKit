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

#ifndef SVGStyleElement_h
#define SVGStyleElement_h

#include "InlineStyleSheetOwner.h"
#include "SVGElement.h"

namespace WebCore {

class SVGStyleElement final : public SVGElement {
public:
    static PassRefPtr<SVGStyleElement> create(const QualifiedName&, Document&, bool createdByParser);
    virtual ~SVGStyleElement();

    CSSStyleSheet* sheet() const { return m_styleSheetOwner.sheet(); }

    bool disabled() const;
    void setDisabled(bool);
                          
    const AtomicString& type() const;
    void setType(const AtomicString&, ExceptionCode&);

    const AtomicString& media() const;
    void setMedia(const AtomicString&, ExceptionCode&);

    String title() const override;
    void setTitle(const AtomicString&, ExceptionCode&);

private:
    SVGStyleElement(const QualifiedName&, Document&, bool createdByParser);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;
    virtual void childrenChanged(const ChildChange&) override;

    virtual bool rendererIsNeeded(const RenderStyle&) override { return false; }

    virtual void finishParsingChildren() override;

    virtual bool isLoading() const { return m_styleSheetOwner.isLoading(); }
    virtual bool sheetLoaded() override { return m_styleSheetOwner.sheetLoaded(document()); }
    virtual void startLoadingDynamicSheet() override { m_styleSheetOwner.startLoadingDynamicSheet(document()); }
    virtual Timer<SVGElement>* svgLoadEventTimer() override { return &m_svgLoadEventTimer; }

    InlineStyleSheetOwner m_styleSheetOwner;
    Timer<SVGElement> m_svgLoadEventTimer;
};

NODE_TYPE_CASTS(SVGStyleElement)

} // namespace WebCore

#endif // SVGStyleElement_h
