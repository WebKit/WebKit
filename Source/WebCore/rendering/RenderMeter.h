/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef RenderMeter_h
#define RenderMeter_h

#if ENABLE(METER_ELEMENT)
#include "RenderBlockFlow.h"
#include "RenderWidget.h"


namespace WebCore {

class HTMLMeterElement;

class RenderMeter final : public RenderBlockFlow {
public:
    RenderMeter(HTMLElement&, Ref<RenderStyle>&&);
    virtual ~RenderMeter();

    HTMLMeterElement* meterElement() const;
    void updateFromElement() override;

private:
    void updateLogicalWidth() override;
    void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    const char* renderName() const override { return "RenderMeter"; }
    bool isMeter() const override { return true; }
    bool requiresForcedStyleRecalcPropagation() const override { return true; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMeter, isMeter())

#endif // ENABLE(METER_ELEMENT)

#endif // RenderMeter_h

