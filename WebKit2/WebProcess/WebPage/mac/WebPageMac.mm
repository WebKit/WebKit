/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebPage.h"

#include "WebEvent.h"
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/WindowsKeyboardCodes.h>

using namespace WebCore;

namespace WebKit {

void WebPage::platformInitialize()
{
    m_page->addSchedulePair(SchedulePair::create([NSRunLoop currentRunLoop], kCFRunLoopCommonModes));
}

void WebPage::platformPreferencesDidChange(const WebPreferencesStore&)
{
}

// FIXME: Editor commands should not be hard coded and instead should come from AppKit.  

static const unsigned CtrlKey   = 1 << 0;
static const unsigned AltKey    = 1 << 1;
static const unsigned ShiftKey  = 1 << 2;
static const unsigned MetaKey   = 1 << 3;

// Keys with special meaning. These will be delegated to the editor using
// the execCommand() method
struct KeyDownEntry {
    unsigned virtualKey;
    unsigned modifiers;
    const char* name;
};

struct KeyPressEntry {
    unsigned charCode;
    unsigned modifiers;
    const char* name;
};

static const KeyDownEntry keyDownEntries[] = {
    { VK_LEFT,     0,                   "MoveLeft"                                      },
    { VK_LEFT,     ShiftKey,            "MoveLeftAndModifySelection"                    },
    { VK_LEFT,     AltKey,              "MoveWordLeft"                                  },
    { VK_LEFT,     AltKey | ShiftKey,   "MoveWordLeftAndModifySelection"                },
    { VK_RIGHT,    0,                   "MoveRight"                                     },
    { VK_RIGHT,    ShiftKey,            "MoveRightAndModifySelection"                   },
    { VK_RIGHT,    AltKey,              "MoveWordRight"                                 },
    { VK_RIGHT,    AltKey | ShiftKey,   "MoveWordRightAndModifySelection"               },
    { VK_UP,       0,                   "MoveUp"                                        },
    { VK_UP,       ShiftKey,            "MoveUpAndModifySelection"                      },
    { VK_PRIOR,    ShiftKey,            "MovePageUpAndModifySelection"                  },
    { VK_DOWN,     0,                   "MoveDown"                                      },
    { VK_DOWN,     ShiftKey,            "MoveDownAndModifySelection"                    },
    { VK_NEXT,     ShiftKey,            "MovePageDownAndModifySelection"                },
    { VK_PRIOR,    0,                   "MovePageUp"                                    },
    { VK_NEXT,     0,                   "MovePageDown"                                  },

    { VK_HOME,     0,                   "MoveToBeginningOfLine"                         },
    { VK_HOME,     ShiftKey,            "MoveToBeginningOfLineAndModifySelection"       },
    { VK_LEFT,     MetaKey,             "MoveToBeginningOfLine"                         },
    { VK_LEFT,     MetaKey | ShiftKey,  "MoveToBeginningOfLineAndModifySelection"       },
    { VK_UP,       MetaKey,             "MoveToBeginningOfDocument"                     },
    { VK_UP,       MetaKey | ShiftKey,  "MoveToBeginningOfDocumentAndModifySelection"   },

    { VK_END,      0,                   "MoveToEndOfLine"                               },
    { VK_END,      ShiftKey,            "MoveToEndOfLineAndModifySelection"             },
    { VK_DOWN,     MetaKey,             "MoveToEndOfDocument"                           },
    { VK_DOWN,     MetaKey | ShiftKey,  "MoveToEndOfDocumentAndModifySelection"         },
    { VK_RIGHT,    MetaKey,             "MoveToEndOfLine"                               },
    { VK_RIGHT,    MetaKey | ShiftKey,  "MoveToEndOfLineAndModifySelection"             },

    { VK_BACK,     0,                   "DeleteBackward"                                },
    { VK_BACK,     ShiftKey,            "DeleteBackward"                                },
    { VK_DELETE,   0,                   "DeleteForward"                                 },
    { VK_BACK,     AltKey,              "DeleteWordBackward"                            },
    { VK_DELETE,   AltKey,              "DeleteWordForward"                             },

    { 'B',         CtrlKey,             "ToggleBold"                                    },
    { 'I',         CtrlKey,             "ToggleItalic"                                  },

    { VK_ESCAPE,   0,                   "Cancel"                                        },
    { VK_OEM_PERIOD, CtrlKey,           "Cancel"                                        },
    { VK_TAB,      0,                   "InsertTab"                                     },
    { VK_TAB,      ShiftKey,            "InsertBacktab"                                 },
    { VK_RETURN,   0,                   "InsertNewline"                                 },
    { VK_RETURN,   CtrlKey,             "InsertNewline"                                 },
    { VK_RETURN,   AltKey,              "InsertNewline"                                 },
    { VK_RETURN,   AltKey | ShiftKey,   "InsertNewline"                                 },
    { VK_RETURN,   ShiftKey,            "InsertLineBreak"                               },

    { 'C',         MetaKey,             "Copy"                                          },
    { 'V',         MetaKey,             "Paste"                                         },
    { 'X',         MetaKey,             "Cut"                                           },
    { 'A',         MetaKey,             "SelectAll"                                     },
    { VK_INSERT,   CtrlKey,             "Copy"                                          },
    { VK_INSERT,   ShiftKey,            "Paste"                                         },
    { VK_DELETE,   ShiftKey,            "Cut"                                           },
    { 'Z',         MetaKey,             "Undo"                                          },
    { 'Z',         MetaKey | ShiftKey,  "Redo"                                          },
};

static const KeyPressEntry keyPressEntries[] = {
    { '\t',        0,                   "InsertTab"                                     },
    { '\t',        ShiftKey,            "InsertBacktab"                                 },
    { '\r',        0,                   "InsertNewline"                                 },
    { '\r',        CtrlKey,             "InsertNewline"                                 },
    { '\r',        ShiftKey,            "InsertLineBreak"                               },
    { '\r',        AltKey,              "InsertNewline"                                 },
    { '\r',        AltKey | ShiftKey,   "InsertNewline"                                 },
};

const char* WebPage::interpretKeyEvent(const KeyboardEvent* evt)
{
    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    ASSERT(keyEvent);

    static HashMap<int, const char*>* keyDownCommandsMap = 0;
    static HashMap<int, const char*>* keyPressCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;
        keyPressCommandsMap = new HashMap<int, const char*>;

        for (unsigned i = 0; i < (sizeof(keyDownEntries) / sizeof(keyDownEntries[0])); i++) {
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey,
                                    keyDownEntries[i].name);
        }

        for (unsigned i = 0; i < (sizeof(keyPressEntries) / sizeof(keyPressEntries[0])); i++) {
            keyPressCommandsMap->set(keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode,
                                     keyPressEntries[i].name);
        }
    }

    unsigned modifiers = 0;
    if (keyEvent->shiftKey())
        modifiers |= ShiftKey;
    if (keyEvent->altKey())
        modifiers |= AltKey;
    if (keyEvent->ctrlKey())
        modifiers |= CtrlKey;
    if (keyEvent->metaKey())
        modifiers |= MetaKey;

    if (keyEvent->type() == PlatformKeyboardEvent::RawKeyDown) {
        int mapKey = modifiers << 16 | evt->keyCode();
        return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
    }

    int mapKey = modifiers << 16 | evt->charCode();
    return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

static inline void scroll(Page* page, ScrollDirection direction, ScrollGranularity granularity)
{
    page->focusController()->focusedOrMainFrame()->eventHandler()->scrollRecursively(direction, granularity);
}

bool WebPage::performDefaultBehaviorForKeyEvent(const WebKeyboardEvent& keyboardEvent)
{
    if (keyboardEvent.type() != WebEvent::KeyDown)
        return false;

    // FIXME: This should be in WebCore.

    switch (keyboardEvent.windowsVirtualKeyCode()) {
    case VK_BACK:
        if (keyboardEvent.shiftKey())
            m_page->goForward();
        else
            m_page->goBack();
        break;
    case VK_SPACE:
        if (keyboardEvent.shiftKey())
            scroll(m_page.get(), ScrollUp, ScrollByPage);
        else
            scroll(m_page.get(), ScrollDown, ScrollByPage);
        break;
    case VK_PRIOR:
        scroll(m_page.get(), ScrollUp, ScrollByPage);
        break;
    case VK_NEXT:
        scroll(m_page.get(), ScrollDown, ScrollByPage);
        break;
    case VK_HOME:
        scroll(m_page.get(), ScrollUp, ScrollByDocument);
        scroll(m_page.get(), ScrollLeft, ScrollByDocument);
        break;
    case VK_END:
        scroll(m_page.get(), ScrollDown, ScrollByDocument);
        scroll(m_page.get(), ScrollLeft, ScrollByDocument);
        break;
    case VK_UP:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey()) {
            scroll(m_page.get(), ScrollUp, ScrollByDocument);
            scroll(m_page.get(), ScrollLeft, ScrollByDocument);
        } else if (keyboardEvent.altKey() || keyboardEvent.controlKey())
            scroll(m_page.get(), ScrollUp, ScrollByPage);
        else
            scroll(m_page.get(), ScrollUp, ScrollByLine);
        break;
    case VK_DOWN:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey()) {
            scroll(m_page.get(), ScrollDown, ScrollByDocument);
            scroll(m_page.get(), ScrollLeft, ScrollByDocument);
        } else if (keyboardEvent.altKey() || keyboardEvent.controlKey())
            scroll(m_page.get(), ScrollDown, ScrollByPage);
        else
            scroll(m_page.get(), ScrollDown, ScrollByLine);
        break;
    case VK_LEFT:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey())
            m_page->goBack();
        else {
            if (keyboardEvent.altKey()  | keyboardEvent.controlKey())
                scroll(m_page.get(), ScrollLeft, ScrollByPage);
            else
                scroll(m_page.get(), ScrollLeft, ScrollByLine);
        }
        break;
    case VK_RIGHT:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey())
            m_page->goForward();
        else {
            if (keyboardEvent.altKey() || keyboardEvent.controlKey())
                scroll(m_page.get(), ScrollRight, ScrollByPage);
            else
                scroll(m_page.get(), ScrollRight, ScrollByLine);
        }
        break;
    default:
        return false;
    }

    return true;
}

} // namespace WebKit
