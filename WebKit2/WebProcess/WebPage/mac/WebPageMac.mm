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

#include <WebCore/KeyboardEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>

using namespace WebCore;

namespace WebKit {

void WebPage::platformInitialize()
{
    m_page->addSchedulePair(SchedulePair::create([NSRunLoop currentRunLoop], kCFRunLoopCommonModes));
}

// FIXME: Editor commands should not be hard coded and instead should come from AppKit.  

static const unsigned CtrlKey   = 1 << 0;
static const unsigned AltKey    = 1 << 1;
static const unsigned ShiftKey  = 1 << 2;
static const unsigned MetaKey   = 1 << 3;

static const unsigned VKEY_BACK         = 0x08;
static const unsigned VKEY_TAB          = 0x09;
static const unsigned VKEY_RETURN       = 0x0D;
static const unsigned VKEY_ESCAPE       = 0x1B;
static const unsigned VKEY_PRIOR        = 0x21;
static const unsigned VKEY_NEXT         = 0x22;
static const unsigned VKEY_END          = 0x23;
static const unsigned VKEY_HOME         = 0x24;
static const unsigned VKEY_LEFT         = 0x25;
static const unsigned VKEY_UP           = 0x26;
static const unsigned VKEY_RIGHT        = 0x27;
static const unsigned VKEY_DOWN         = 0x28;
static const unsigned VKEY_INSERT       = 0x2D;
static const unsigned VKEY_DELETE       = 0x2E;
static const unsigned VKEY_OEM_PERIOD   = 0xBE;

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
    { VKEY_LEFT,   0,                   "MoveLeft"                                      },
    { VKEY_LEFT,   ShiftKey,            "MoveLeftAndModifySelection"                    },
    { VKEY_LEFT,   AltKey,              "MoveWordLeft"                                  },
    { VKEY_LEFT,   AltKey | ShiftKey,   "MoveWordLeftAndModifySelection"                },
    { VKEY_RIGHT,  0,                   "MoveRight"                                     },
    { VKEY_RIGHT,  ShiftKey,            "MoveRightAndModifySelection"                   },
    { VKEY_RIGHT,  AltKey,              "MoveWordRight"                                 },
    { VKEY_RIGHT,  AltKey | ShiftKey,   "MoveWordRightAndModifySelection"               },
    { VKEY_UP,     0,                   "MoveUp"                                        },
    { VKEY_UP,     ShiftKey,            "MoveUpAndModifySelection"                      },
    { VKEY_PRIOR,  ShiftKey,            "MovePageUpAndModifySelection"                  },
    { VKEY_DOWN,   0,                   "MoveDown"                                      },
    { VKEY_DOWN,   ShiftKey,            "MoveDownAndModifySelection"                    },
    { VKEY_NEXT,   ShiftKey,            "MovePageDownAndModifySelection"                },
    { VKEY_PRIOR,  0,                   "MovePageUp"                                    },
    { VKEY_NEXT,   0,                   "MovePageDown"                                  },

    { VKEY_HOME,   0,                   "MoveToBeginningOfLine"                         },
    { VKEY_HOME,   ShiftKey,            "MoveToBeginningOfLineAndModifySelection"       },
    { VKEY_LEFT,   MetaKey,             "MoveToBeginningOfLine"                         },
    { VKEY_LEFT,   MetaKey | ShiftKey,  "MoveToBeginningOfLineAndModifySelection"       },
    { VKEY_UP,     MetaKey,             "MoveToBeginningOfDocument"                     },
    { VKEY_UP,     MetaKey | ShiftKey,  "MoveToBeginningOfDocumentAndModifySelection"   },

    { VKEY_END,    0,                   "MoveToEndOfLine"                               },
    { VKEY_END,    ShiftKey,            "MoveToEndOfLineAndModifySelection"             },
    { VKEY_DOWN,   MetaKey,             "MoveToEndOfDocument"                           },
    { VKEY_DOWN,   MetaKey | ShiftKey,  "MoveToEndOfDocumentAndModifySelection"         },
    { VKEY_RIGHT,  MetaKey,             "MoveToEndOfLine"                               },
    { VKEY_RIGHT,  MetaKey | ShiftKey,  "MoveToEndOfLineAndModifySelection"             },

    { VKEY_BACK,   0,                   "DeleteBackward"                                },
    { VKEY_BACK,   ShiftKey,            "DeleteBackward"                                },
    { VKEY_DELETE, 0,                   "DeleteForward"                                 },
    { VKEY_BACK,   AltKey,              "DeleteWordBackward"                            },
    { VKEY_DELETE, AltKey,              "DeleteWordForward"                             },

    { 'B',         CtrlKey,             "ToggleBold"                                    },
    { 'I',         CtrlKey,             "ToggleItalic"                                  },

    { VKEY_ESCAPE, 0,                   "Cancel"                                        },
    { VKEY_OEM_PERIOD, CtrlKey,         "Cancel"                                        },
    { VKEY_TAB,    0,                   "InsertTab"                                     },
    { VKEY_TAB,    ShiftKey,            "InsertBacktab"                                 },
    { VKEY_RETURN, 0,                   "InsertNewline"                                 },
    { VKEY_RETURN, CtrlKey,             "InsertNewline"                                 },
    { VKEY_RETURN, AltKey,              "InsertNewline"                                 },
    { VKEY_RETURN, AltKey | ShiftKey,   "InsertNewline"                                 },
    { VKEY_RETURN, ShiftKey,            "InsertLineBreak"                               },

    { 'C',         MetaKey,             "Copy"                                          },
    { 'V',         MetaKey,             "Paste"                                         },
    { 'X',         MetaKey,             "Cut"                                           },
    { 'A',         MetaKey,             "SelectAll"                                     },
    { VKEY_INSERT, CtrlKey,             "Copy"                                          },
    { VKEY_INSERT, ShiftKey,            "Paste"                                         },
    { VKEY_DELETE, ShiftKey,            "Cut"                                           },
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

} // namespace WebKit
