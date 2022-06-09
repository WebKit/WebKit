/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "LabelableElement.h"

namespace WebCore {

class ProgressValueElement;
class RenderProgress;

class HTMLProgressElement final : public LabelableElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLProgressElement);
public:
    static const double IndeterminatePosition;
    static const double InvalidPosition;

    static Ref<HTMLProgressElement> create(const QualifiedName&, Document&);

    double value() const;
    void setValue(double);

    double max() const;
    void setMax(double);

    double position() const;

private:
    HTMLProgressElement(const QualifiedName&, Document&);
    virtual ~HTMLProgressElement();

    bool shouldAppearIndeterminate() const final;
    bool supportLabels() const final { return true; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool childShouldCreateRenderer(const Node&) const final;
    RenderProgress* renderProgress() const;

    void parseAttribute(const QualifiedName&, const AtomString&) final;

    void didAttachRenderers() final;

    void didElementStateChange();
    void didAddUserAgentShadowRoot(ShadowRoot&) final;
    bool isDeterminate() const;

    bool canContainRangeEndPoint() const final { return false; }

    ProgressValueElement* m_value;
};

} // namespace
