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

#include "AccessibilityRenderObject.h"

namespace WebCore {

class HTMLMeterElement;
class RenderMeter;

class HTMLProgressElement;
class RenderProgress;
    
class AccessibilityProgressIndicator final : public AccessibilityRenderObject {
public:
    static Ref<AccessibilityProgressIndicator> create(RenderObject*);

    bool isIndeterminate() const final;

private:
    AccessibilityRole determineAccessibilityRole() final;

    String valueDescription() const override;
    String gaugeRegionValueDescription() const;
    float valueForRange() const override;
    float maxValueForRange() const override;
    float minValueForRange() const override;

    explicit AccessibilityProgressIndicator(RenderObject*);
    HTMLProgressElement* progressElement() const;
    HTMLMeterElement* meterElement() const;
    
    bool computeAccessibilityIsIgnored() const override;
};

} // namespace WebCore
