/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DataListButtonElement.h"

#include "Event.h"
#include "EventNames.h"
#include "MouseEvent.h"
#include "ResolvedStyle.h"
#include "StyleAppearance.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DataListButtonElement);

Ref<DataListButtonElement> DataListButtonElement::create(Document& document, DataListButtonOwner& owner)
{
    return adoptRef(*new DataListButtonElement(document, owner));
}

DataListButtonElement::DataListButtonElement(Document& document, DataListButtonOwner& owner)
    : HTMLDivElement(document, TypeFlag::HasCustomStyleResolveCallbacks)
    , m_owner(owner)
{
}

DataListButtonElement::~DataListButtonElement() = default;

void DataListButtonElement::defaultEventHandler(Event& event)
{
    auto* mouseEvent = dynamicDowncast<MouseEvent>(event);
    if (!mouseEvent) {
        if (!event.defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    if (isAnyClick(*mouseEvent)) {
        m_owner.dataListButtonElementWasClicked();
        event.setDefaultHandled();
    }

    if (!event.defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

bool DataListButtonElement::isDisabledFormControl() const
{
    RefPtr host = shadowHost();
    return host && host->isDisabledFormControl();
}

std::optional<Style::UnadjustedStyle> DataListButtonElement::resolveCustomStyle(const Style::ResolutionContext& resolutionContext, const RenderStyle* shadowHostStyle)
{
    m_canAdjustStyleForAppearance = true;

    if (!shadowHostStyle)
        return std::nullopt;

    auto usedAppearance = shadowHostStyle->usedAppearance();
    if (usedAppearance == StyleAppearance::None) {
        m_canAdjustStyleForAppearance = false;
        return resolveStyle(resolutionContext);
    }

    return std::nullopt;
}

} // namespace WebCore
