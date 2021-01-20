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

class MeterValueElement;
class RenderMeter;

class HTMLMeterElement final : public LabelableElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLMeterElement);
public:
    static Ref<HTMLMeterElement> create(const QualifiedName&, Document&);

    enum GaugeRegion {
        GaugeRegionOptimum,
        GaugeRegionSuboptimal,
        GaugeRegionEvenLessGood
    };

    double min() const;
    void setMin(double);

    double max() const;
    void setMax(double);

    double value() const;
    void setValue(double);

    double low() const;
    void setLow(double);

    double high() const;
    void setHigh(double);

    double optimum() const;
    void setOptimum(double);

    double valueRatio() const;
    GaugeRegion gaugeRegion() const;

    bool canContainRangeEndPoint() const final { return false; }

private:
    HTMLMeterElement(const QualifiedName&, Document&);
    virtual ~HTMLMeterElement();

    RenderMeter* renderMeter() const;

    bool supportLabels() const final { return true; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool childShouldCreateRenderer(const Node&) const final;
    void parseAttribute(const QualifiedName&, const AtomString&) final;

    void didElementStateChange();
    void didAddUserAgentShadowRoot(ShadowRoot&) final;

    RefPtr<HTMLElement> m_value;
};

} // namespace WebCore
