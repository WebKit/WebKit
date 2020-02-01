/*
 *  Copyright (C) 2019 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "WebEditorClient.h"

#include <WebCore/CompositionHighlight.h>
#include <WebCore/Document.h>
#include <WebCore/Editor.h>
#include <WebCore/Frame.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebPage.h>

namespace WebKit {
using namespace WebCore;

void WebEditorClient::handleInputMethodKeydown(KeyboardEvent& event)
{
    auto* platformEvent = event.underlyingPlatformEvent();
    if (platformEvent && platformEvent->handledByInputMethod())
        event.setDefaultHandled();
}

void WebEditorClient::didDispatchInputMethodKeydown(KeyboardEvent& event)
{
    auto* platformEvent = event.underlyingPlatformEvent();
    ASSERT(event.target());
    auto* frame = downcast<Node>(event.target())->document().frame();
    ASSERT(frame);

    if (const auto& underlines = platformEvent->preeditUnderlines()) {
        auto rangeStart = platformEvent->preeditSelectionRangeStart().valueOr(0);
        auto rangeLength = platformEvent->preeditSelectionRangeLength().valueOr(0);
        frame->editor().setComposition(platformEvent->text(), underlines.value(), { }, rangeStart, rangeStart + rangeLength);
    } else
        frame->editor().confirmComposition(platformEvent->text());
}

} // namespace WebKit
