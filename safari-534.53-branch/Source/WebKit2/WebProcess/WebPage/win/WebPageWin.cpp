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

#include "config.h"
#include "WebPage.h"

#include "FontSmoothingLevel.h"
#include "WebCoreArgumentCoders.h"
#include "WebEvent.h"
#include "WebPageProxyMessages.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include <WebCore/FocusController.h>
#include <WebCore/FontRenderingMode.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/HitTestRequest.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderView.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/Settings.h>
#if USE(CG)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif
#include <WinUser.h>

#if USE(CFNETWORK)
#include <CFNetwork/CFURLCachePriv.h>
#include <CFNetwork/CFURLProtocolPriv.h>
#include <CFNetwork/CFURLRequestPriv.h>
#endif

using namespace WebCore;

namespace WebKit {

void WebPage::platformInitialize()
{
}

void WebPage::platformPreferencesDidChange(const WebPreferencesStore& store)
{
    FontSmoothingLevel fontSmoothingLevel = static_cast<FontSmoothingLevel>(store.getUInt32ValueForKey(WebPreferencesKey::fontSmoothingLevelKey()));

#if USE(CG)
    FontSmoothingLevel adjustedLevel = fontSmoothingLevel;
    if (adjustedLevel == FontSmoothingLevelWindows)
        adjustedLevel = FontSmoothingLevelMedium;
    wkSetFontSmoothingLevel(adjustedLevel);
#endif

    m_page->settings()->setFontRenderingMode(fontSmoothingLevel == FontSmoothingLevelWindows ? AlternateRenderingMode : NormalRenderingMode);
}

static const unsigned CtrlKey = 1 << 0;
static const unsigned AltKey = 1 << 1;
static const unsigned ShiftKey = 1 << 2;

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
    { VK_LEFT,   0,                  "MoveLeft"                                    },
    { VK_LEFT,   ShiftKey,           "MoveLeftAndModifySelection"                  },
    { VK_LEFT,   CtrlKey,            "MoveWordLeft"                                },
    { VK_LEFT,   CtrlKey | ShiftKey, "MoveWordLeftAndModifySelection"              },
    { VK_RIGHT,  0,                  "MoveRight"                                   },
    { VK_RIGHT,  ShiftKey,           "MoveRightAndModifySelection"                 },
    { VK_RIGHT,  CtrlKey,            "MoveWordRight"                               },
    { VK_RIGHT,  CtrlKey | ShiftKey, "MoveWordRightAndModifySelection"             },
    { VK_UP,     0,                  "MoveUp"                                      },
    { VK_UP,     ShiftKey,           "MoveUpAndModifySelection"                    },
    { VK_PRIOR,  ShiftKey,           "MovePageUpAndModifySelection"                },
    { VK_DOWN,   0,                  "MoveDown"                                    },
    { VK_DOWN,   ShiftKey,           "MoveDownAndModifySelection"                  },
    { VK_NEXT,   ShiftKey,           "MovePageDownAndModifySelection"              },
    { VK_PRIOR,  0,                  "MovePageUp"                                  },
    { VK_NEXT,   0,                  "MovePageDown"                                },
    { VK_HOME,   0,                  "MoveToBeginningOfLine"                       },
    { VK_HOME,   ShiftKey,           "MoveToBeginningOfLineAndModifySelection"     },
    { VK_HOME,   CtrlKey,            "MoveToBeginningOfDocument"                   },
    { VK_HOME,   CtrlKey | ShiftKey, "MoveToBeginningOfDocumentAndModifySelection" },

    { VK_END,    0,                  "MoveToEndOfLine"                             },
    { VK_END,    ShiftKey,           "MoveToEndOfLineAndModifySelection"           },
    { VK_END,    CtrlKey,            "MoveToEndOfDocument"                         },
    { VK_END,    CtrlKey | ShiftKey, "MoveToEndOfDocumentAndModifySelection"       },

    { VK_BACK,   0,                  "DeleteBackward"                              },
    { VK_BACK,   ShiftKey,           "DeleteBackward"                              },
    { VK_DELETE, 0,                  "DeleteForward"                               },
    { VK_BACK,   CtrlKey,            "DeleteWordBackward"                          },
    { VK_DELETE, CtrlKey,            "DeleteWordForward"                           },
    
    { 'B',       CtrlKey,            "ToggleBold"                                  },
    { 'I',       CtrlKey,            "ToggleItalic"                                },

    { VK_ESCAPE, 0,                  "Cancel"                                      },
    { VK_OEM_PERIOD, CtrlKey,        "Cancel"                                      },
    { VK_TAB,    0,                  "InsertTab"                                   },
    { VK_TAB,    ShiftKey,           "InsertBacktab"                               },
    { VK_RETURN, 0,                  "InsertNewline"                               },
    { VK_RETURN, CtrlKey,            "InsertNewline"                               },
    { VK_RETURN, AltKey,             "InsertNewline"                               },
    { VK_RETURN, ShiftKey,           "InsertNewline"                               },
    { VK_RETURN, AltKey | ShiftKey,  "InsertNewline"                               },

    // It's not quite clear whether clipboard shortcuts and Undo/Redo should be handled
    // in the application or in WebKit. We chose WebKit.
    { 'C',       CtrlKey,            "Copy"                                        },
    { 'V',       CtrlKey,            "Paste"                                       },
    { 'X',       CtrlKey,            "Cut"                                         },
    { 'A',       CtrlKey,            "SelectAll"                                   },
    { VK_INSERT, CtrlKey,            "Copy"                                        },
    { VK_DELETE, ShiftKey,           "Cut"                                         },
    { VK_INSERT, ShiftKey,           "Paste"                                       },
    { 'Z',       CtrlKey,            "Undo"                                        },
    { 'Z',       CtrlKey | ShiftKey, "Redo"                                        },
};

static const KeyPressEntry keyPressEntries[] = {
    { '\t',   0,                  "InsertTab"                                   },
    { '\t',   ShiftKey,           "InsertBacktab"                               },
    { '\r',   0,                  "InsertNewline"                               },
    { '\r',   CtrlKey,            "InsertNewline"                               },
    { '\r',   AltKey,             "InsertNewline"                               },
    { '\r',   ShiftKey,           "InsertNewline"                               },
    { '\r',   AltKey | ShiftKey,  "InsertNewline"                               },
};

const char* WebPage::interpretKeyEvent(const KeyboardEvent* evt)
{
    ASSERT(evt->type() == eventNames().keydownEvent || evt->type() == eventNames().keypressEvent);

    static HashMap<int, const char*>* keyDownCommandsMap = 0;
    static HashMap<int, const char*>* keyPressCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;
        keyPressCommandsMap = new HashMap<int, const char*>;

        for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyDownEntries); ++i)
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey, keyDownEntries[i].name);

        for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyPressEntries); ++i)
            keyPressCommandsMap->set(keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode, keyPressEntries[i].name);
    }

    unsigned modifiers = 0;
    if (evt->shiftKey())
        modifiers |= ShiftKey;
    if (evt->altKey())
        modifiers |= AltKey;
    if (evt->ctrlKey())
        modifiers |= CtrlKey;

    if (evt->type() == eventNames().keydownEvent) {
        int mapKey = modifiers << 16 | evt->keyCode();
        return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
    }

    int mapKey = modifiers << 16 | evt->charCode();
    return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

bool WebPage::performDefaultBehaviorForKeyEvent(const WebKeyboardEvent& keyboardEvent)
{
    if (keyboardEvent.type() != WebEvent::KeyDown && keyboardEvent.type() != WebEvent::RawKeyDown)
        return false;

    switch (keyboardEvent.windowsVirtualKeyCode()) {
    case VK_BACK:
        if (keyboardEvent.isSystemKey())
            return false;
        if (keyboardEvent.shiftKey())
            m_page->goForward();
        else
            m_page->goBack();
        break;
    case VK_LEFT:
        if (keyboardEvent.isSystemKey())
            m_page->goBack();
        else
            scroll(m_page.get(), ScrollLeft, ScrollByLine);
        break;
    case VK_RIGHT:
        if (keyboardEvent.isSystemKey())
            m_page->goForward();
        else
            scroll(m_page.get(), ScrollRight, ScrollByLine);
        break;
    case VK_UP:
        if (keyboardEvent.isSystemKey())
            return false;
        scroll(m_page.get(), ScrollUp, ScrollByLine);
        break;
    case VK_DOWN:
        if (keyboardEvent.isSystemKey())
            return false;
        scroll(m_page.get(), ScrollDown, ScrollByLine);
        break;
    case VK_HOME:
        if (keyboardEvent.isSystemKey())
            return false;
        logicalScroll(m_page.get(), ScrollBlockDirectionBackward, ScrollByDocument);
        break;
    case VK_END:
        if (keyboardEvent.isSystemKey())
            return false;
        logicalScroll(m_page.get(), ScrollBlockDirectionForward, ScrollByDocument);
        break;
    case VK_PRIOR:
        if (keyboardEvent.isSystemKey())
            return false;
        logicalScroll(m_page.get(), ScrollBlockDirectionBackward, ScrollByPage);
        break;
    case VK_NEXT:
        if (keyboardEvent.isSystemKey())
            return false;
        logicalScroll(m_page.get(), ScrollBlockDirectionForward, ScrollByPage);
        break;
    default:
        return false;
    }

    return true;
}

#if USE(CFNETWORK)
static RetainPtr<CFCachedURLResponseRef> cachedResponseForURL(WebPage* webPage, const KURL& url)
{
    RetainPtr<CFURLRef> cfURL(AdoptCF, url.createCFURL());
    RetainPtr<CFMutableURLRequestRef> request(AdoptCF, CFURLRequestCreateMutable(0, cfURL.get(), kCFURLRequestCachePolicyReloadIgnoringCache, 60, 0));
#if USE(CFURLSTORAGESESSIONS)
    wkSetRequestStorageSession(ResourceHandle::currentStorageSession(), request.get());
#endif

    RetainPtr<CFStringRef> userAgent(AdoptCF, webPage->userAgent().createCFString());
    CFURLRequestSetHTTPHeaderFieldValue(request.get(), CFSTR("User-Agent"), userAgent.get());

    RetainPtr<CFURLCacheRef> cache;
#if USE(CFURLSTORAGESESSIONS)
    if (CFURLStorageSessionRef currentStorageSession = ResourceHandle::currentStorageSession())
        cache.adoptCF(wkCopyURLCache(currentStorageSession));
    else
#endif
        cache.adoptCF(CFURLCacheCopySharedURLCache());

    RetainPtr<CFCachedURLResponseRef> response(AdoptCF, CFURLCacheCopyResponseForRequest(cache.get(), request.get()));
    return response;        
}
#endif

bool WebPage::platformHasLocalDataForURL(const KURL& url)
{
#if USE(CFNETWORK)
    return cachedResponseForURL(this, url);
#else
    return false;
#endif
}

String WebPage::cachedResponseMIMETypeForURL(const KURL& url)
{
#if USE(CFNETWORK)
    RetainPtr<CFCachedURLResponseRef> cachedResponse = cachedResponseForURL(this, url);
    CFURLResponseRef response = CFCachedURLResponseGetWrappedResponse(cachedResponse.get());
    return response ? CFURLResponseGetMIMEType(response) : String();
#else
    return String();
#endif
}

String WebPage::cachedSuggestedFilenameForURL(const KURL& url)
{
#if USE(CFNETWORK)
    RetainPtr<CFCachedURLResponseRef> cachedResponse = cachedResponseForURL(this, url);
    CFURLResponseRef response = CFCachedURLResponseGetWrappedResponse(cachedResponse.get());
    if (!response)
        return String();
    RetainPtr<CFStringRef> suggestedFilename(AdoptCF, CFURLResponseCopySuggestedFilename(response));

    return suggestedFilename.get();
#else
    return String();
#endif
}

PassRefPtr<SharedBuffer> WebPage::cachedResponseDataForURL(const KURL& url)
{
#if USE(CFNETWORK)
    RetainPtr<CFCachedURLResponseRef> cachedResponse = cachedResponseForURL(this, url);
    CFDataRef data = CFCachedURLResponseGetReceiverData(cachedResponse.get());
    if (!data)
        return 0;

    return SharedBuffer::wrapCFData(data);
#else
    return 0;
#endif
}

bool WebPage::platformCanHandleRequest(const WebCore::ResourceRequest& request)
{
#if USE(CFNETWORK)
    return CFURLProtocolCanHandleRequest(request.cfURLRequest());
#else
    return true;
#endif
}

void WebPage::confirmComposition(const String& compositionString)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame || !frame->editor()->canEdit())
        return;
    frame->editor()->confirmComposition(compositionString);
}

void WebPage::setComposition(const String& compositionString, const Vector<WebCore::CompositionUnderline>& underlines, uint64_t cursorPosition)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame || !frame->editor()->canEdit())
        return;
    frame->editor()->setComposition(compositionString, underlines, cursorPosition, 0);
}

void WebPage::firstRectForCharacterInSelectedRange(const uint64_t characterPosition, WebCore::IntRect& resultRect)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    IntRect rect;
    if (RefPtr<Range> range = frame->editor()->hasComposition() ? frame->editor()->compositionRange() : frame->selection()->selection().toNormalizedRange()) {
        ExceptionCode ec = 0;
        RefPtr<Range> tempRange = range->cloneRange(ec);
        tempRange->setStart(tempRange->startContainer(ec), tempRange->startOffset(ec) + characterPosition, ec);
        rect = frame->editor()->firstRectForRange(tempRange.get());
    }
    resultRect = frame->view()->contentsToWindow(rect);
}

void WebPage::getSelectedText(String& text)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    RefPtr<Range> selectedRange = frame->selection()->toNormalizedRange();
    text = selectedRange->text();
}

void WebPage::gestureWillBegin(const WebCore::IntPoint& point, bool& canBeginPanning)
{
    m_gestureReachedScrollingLimit = false;

    bool hitScrollbar = false;

    HitTestRequest request(HitTestRequest::ReadOnly);
    for (Frame* childFrame = m_page->mainFrame(); childFrame; childFrame = EventHandler::subframeForTargetNode(m_gestureTargetNode.get())) {
        ScrollView* scollView = childFrame->view();
        if (!scollView)
            break;
        
        RenderView* renderView = childFrame->document()->renderView();
        if (!renderView)
            break;

        RenderLayer* layer = renderView->layer();
        if (!layer)
            break;

        HitTestResult result = scollView->windowToContents(point);
        layer->hitTest(request, result);
        m_gestureTargetNode = result.innerNode();

        if (!hitScrollbar)
            hitScrollbar = result.scrollbar();
    }

    if (hitScrollbar) {
        canBeginPanning = false;
        return;
    }

    if (!m_gestureTargetNode) {
        canBeginPanning = false;
        return;
    }

    for (RenderObject* renderer = m_gestureTargetNode->renderer(); renderer; renderer = renderer->parent()) {
        if (renderer->isBox() && toRenderBox(renderer)->canBeScrolledAndHasScrollableArea()) {
            canBeginPanning = true;
            return;
        }
    }

    canBeginPanning = false;
}

static bool scrollbarAtTopOrBottomOfDocument(Scrollbar* scrollbar)
{
    ASSERT_ARG(scrollbar, scrollbar);
    return !scrollbar->currentPos() || scrollbar->currentPos() >= scrollbar->maximum();
}

void WebPage::gestureDidScroll(const IntSize& size)
{
    ASSERT_ARG(size, !size.isZero());

    if (!m_gestureTargetNode || !m_gestureTargetNode->renderer() || !m_gestureTargetNode->renderer()->enclosingLayer())
        return;

    Scrollbar* verticalScrollbar = 0;
    if (Frame* frame = m_page->mainFrame()) {
        if (ScrollView* view = frame->view())
            verticalScrollbar = view->verticalScrollbar();
    }

    m_gestureTargetNode->renderer()->enclosingLayer()->scrollByRecursively(size.width(), size.height());
    bool gestureReachedScrollingLimit = verticalScrollbar && scrollbarAtTopOrBottomOfDocument(verticalScrollbar);

    // FIXME: We really only want to update this state if the state was updated via scrolling the main frame,
    // not scrolling something in a main frame when the main frame had already reached its scrolling limit.

    if (gestureReachedScrollingLimit == m_gestureReachedScrollingLimit)
        return;

    send(Messages::WebPageProxy::SetGestureReachedScrollingLimit(gestureReachedScrollingLimit));
    m_gestureReachedScrollingLimit = gestureReachedScrollingLimit;
}

void WebPage::gestureDidEnd()
{
    m_gestureTargetNode = nullptr;
}

} // namespace WebKit
