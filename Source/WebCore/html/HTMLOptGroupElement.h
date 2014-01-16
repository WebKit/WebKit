/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLOptGroupElement_h
#define HTMLOptGroupElement_h

#include "HTMLElement.h"

namespace WebCore {
    
class HTMLSelectElement;

class HTMLOptGroupElement final : public HTMLElement {
public:
    static PassRefPtr<HTMLOptGroupElement> create(const QualifiedName&, Document&);

    virtual bool isDisabledFormControl() const override;
    HTMLSelectElement* ownerSelectElement() const;
    
    String groupLabelText() const;

private:
    HTMLOptGroupElement(const QualifiedName&, Document&);

    virtual const AtomicString& formControlType() const;
    virtual bool isFocusable() const override;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool rendererIsNeeded(const RenderStyle&) override { return false; }
    virtual void didAttachRenderers() override;
    virtual void willDetachRenderers() override;

    virtual void childrenChanged(const ChildChange&) override;

    virtual void accessKeyAction(bool sendMouseEvents) override;

    // <optgroup> never has a renderer so we manually manage a cached style.
    void updateNonRenderStyle();
    virtual RenderStyle* nonRendererStyle() const override;
    virtual PassRefPtr<RenderStyle> customStyleForRenderer() override;

    void recalcSelectOptions();

    RefPtr<RenderStyle> m_style;
};

NODE_TYPE_CASTS(HTMLOptGroupElement)

} //namespace

#endif
