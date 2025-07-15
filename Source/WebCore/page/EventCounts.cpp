/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "EventCounts.h"
#include "IDLTypes.h"
#include "JSDOMMapLike.h"
#include <algorithm>
#include <initializer_list>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static constexpr std::array COUNTED_EVENT_SET {
    EventType::auxclick,
    EventType::click,
    EventType::contextmenu,
    EventType::dblclick,
    EventType::mousedown,
    EventType::mouseenter,
    EventType::mouseleave,
    EventType::mouseout,
    EventType::mouseover,
    EventType::mouseup,
    EventType::pointerover,
    EventType::pointerenter,
    EventType::pointerdown,
    EventType::pointerup,
    EventType::pointercancel,
    EventType::pointerout,
    EventType::pointerleave,
    EventType::gotpointercapture,
    EventType::lostpointercapture,
    EventType::touchstart,
    EventType::touchend,
    EventType::touchcancel,
    EventType::keydown,
    EventType::keypress,
    EventType::keyup,
    EventType::beforeinput,
    EventType::input,
    EventType::compositionstart,
    EventType::compositionupdate,
    EventType::compositionend,
    EventType::dragstart,
    EventType::dragend,
    EventType::dragenter,
    EventType::dragleave,
    EventType::dragover,
    EventType::drop
};

EventCounts::EventCounts()
{ }

void EventCounts::add(EventType t)
{
    auto index = eventTypeIndex(t);
    ASSERT(index);
    ++m_counts[index.value()];
}

std::optional<size_t> EventCounts::eventTypeIndex(EventType t)
{
    auto it = std::ranges::find(COUNTED_EVENT_SET, t);
    if (COUNTED_EVENT_SET.end() == it)
        return std::nullopt;

    return std::distance(COUNTED_EVENT_SET.begin(), it);
}

bool EventCounts::IsCounted(EventType t)
{
    return !eventNameFromType(t).isNull();
}

const AtomString& EventCounts::eventNameFromType(EventType t)
{
    static const WTF::NeverDestroyed map(std::map<EventType, const AtomString>({
        { EventType::auxclick           , eventNames().auxclickEvent           },
        { EventType::click              , eventNames().clickEvent              },
        { EventType::contextmenu        , eventNames().contextmenuEvent        },
        { EventType::dblclick           , eventNames().dblclickEvent           },
        { EventType::mousedown          , eventNames().mousedownEvent          },
        { EventType::mouseenter         , eventNames().mouseenterEvent         },
        { EventType::mouseleave         , eventNames().mouseleaveEvent         },
        { EventType::mouseout           , eventNames().mouseoutEvent           },
        { EventType::mouseover          , eventNames().mouseoverEvent          },
        { EventType::mouseup            , eventNames().mouseupEvent            },
        { EventType::pointerover        , eventNames().pointeroverEvent        },
        { EventType::pointerenter       , eventNames().pointerenterEvent       },
        { EventType::pointerdown        , eventNames().pointerdownEvent        },
        { EventType::pointerup          , eventNames().pointerupEvent          },
        { EventType::pointercancel      , eventNames().pointercancelEvent      },
        { EventType::pointerout         , eventNames().pointeroutEvent         },
        { EventType::pointerleave       , eventNames().pointerleaveEvent       },
        { EventType::gotpointercapture  , eventNames().gotpointercaptureEvent  },
        { EventType::lostpointercapture , eventNames().lostpointercaptureEvent },
        { EventType::touchstart         , eventNames().touchstartEvent         },
        { EventType::touchend           , eventNames().touchendEvent           },
        { EventType::touchcancel        , eventNames().touchcancelEvent        },
        { EventType::keydown            , eventNames().keydownEvent            },
        { EventType::keypress           , eventNames().keypressEvent           },
        { EventType::keyup              , eventNames().keyupEvent              },
        { EventType::beforeinput        , eventNames().beforeinputEvent        },
        { EventType::input              , eventNames().inputEvent              },
        { EventType::compositionstart   , eventNames().compositionstartEvent   },
        { EventType::compositionupdate  , eventNames().compositionupdateEvent  },
        { EventType::compositionend     , eventNames().compositionendEvent     },
        { EventType::dragstart          , eventNames().dragstartEvent          },
        { EventType::dragend            , eventNames().dragendEvent            },
        { EventType::dragenter          , eventNames().dragenterEvent          },
        { EventType::dragleave          , eventNames().dragleaveEvent          },
        { EventType::dragover           , eventNames().dragoverEvent           },
        { EventType::drop               , eventNames().dropEvent               }
    }));

    static const NeverDestroyed<AtomString> null;
    if (!map->contains(t))
        return null;

    return map->at(t);
}

void EventCounts::initializeMapLike(DOMMapAdapter& map)
{
    // TODO: this implementation does not work as expected: a new maplike object
    // is created only once, causing it to become out-of-sync with m_counts
    for (size_t idx = 0; idx < COUNTED_EVENT_SET.size(); idx++) {
        auto type = COUNTED_EVENT_SET[idx];
        map.set<IDLDOMString, IDLUnsignedLongLong>(eventNameFromType(type), m_counts[idx]);
    }
}

} // namespace WebCore
