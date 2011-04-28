/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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
#include "PageClientImpl.h"

#include "ChunkedUpdateDrawingAreaProxy.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NotImplemented.h"
#include "WebContext.h"
#include "WebContextMenuProxy.h"
#include "WebEventFactory.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageProxy.h"
#include <wtf/text/WTFString.h>

typedef HashMap<int, const char*> IntConstCharHashMap;

using namespace WebCore;

namespace WebKit {

static void backspaceCallback(GtkWidget* widget, PageClientImpl* client)
{
    g_signal_stop_emission_by_name(widget, "backspace");
    client->addPendingEditorCommand("DeleteBackward");
}

static void selectAllCallback(GtkWidget* widget, gboolean select, PageClientImpl* client)
{
    g_signal_stop_emission_by_name(widget, "select-all");
    client->addPendingEditorCommand(select ? "SelectAll" : "Unselect");
}

static void cutClipboardCallback(GtkWidget* widget, PageClientImpl* client)
{
    g_signal_stop_emission_by_name(widget, "cut-clipboard");
    client->addPendingEditorCommand("Cut");
}

static void copyClipboardCallback(GtkWidget* widget, PageClientImpl* client)
{
    g_signal_stop_emission_by_name(widget, "copy-clipboard");
    client->addPendingEditorCommand("Copy");
}

static void pasteClipboardCallback(GtkWidget* widget, PageClientImpl* client)
{
    g_signal_stop_emission_by_name(widget, "paste-clipboard");
    client->addPendingEditorCommand("Paste");
}

static void toggleOverwriteCallback(GtkWidget* widget, EditorClient*)
{
    // We don't support toggling the overwrite mode, but the default callback expects
    // the GtkTextView to have a layout, so we handle this signal just to stop it.
    g_signal_stop_emission_by_name(widget, "toggle-overwrite");
}

// GTK+ will still send these signals to the web view. So we can safely stop signal
// emission without breaking accessibility.
static void popupMenuCallback(GtkWidget* widget, EditorClient*)
{
    g_signal_stop_emission_by_name(widget, "popup-menu");
}

static void showHelpCallback(GtkWidget* widget, EditorClient*)
{
    g_signal_stop_emission_by_name(widget, "show-help");
}

static const char* const gtkDeleteCommands[][2] = {
    { "DeleteBackward",               "DeleteForward"                        }, // Characters
    { "DeleteWordBackward",           "DeleteWordForward"                    }, // Word ends
    { "DeleteWordBackward",           "DeleteWordForward"                    }, // Words
    { "DeleteToBeginningOfLine",      "DeleteToEndOfLine"                    }, // Lines
    { "DeleteToBeginningOfLine",      "DeleteToEndOfLine"                    }, // Line ends
    { "DeleteToBeginningOfParagraph", "DeleteToEndOfParagraph"               }, // Paragraph ends
    { "DeleteToBeginningOfParagraph", "DeleteToEndOfParagraph"               }, // Paragraphs
    { 0,                              0                                      } // Whitespace (M-\ in Emacs)
};

static void deleteFromCursorCallback(GtkWidget* widget, GtkDeleteType deleteType, gint count, PageClientImpl* client)
{
    g_signal_stop_emission_by_name(widget, "delete-from-cursor");
    int direction = count > 0 ? 1 : 0;

    // Ensuring that deleteType <= G_N_ELEMENTS here results in a compiler warning
    // that the condition is always true.

    if (deleteType == GTK_DELETE_WORDS) {
        if (!direction) {
            client->addPendingEditorCommand("MoveWordForward");
            client->addPendingEditorCommand("MoveWordBackward");
        } else {
            client->addPendingEditorCommand("MoveWordBackward");
            client->addPendingEditorCommand("MoveWordForward");
        }
    } else if (deleteType == GTK_DELETE_DISPLAY_LINES) {
        if (!direction)
            client->addPendingEditorCommand("MoveToBeginningOfLine");
        else
            client->addPendingEditorCommand("MoveToEndOfLine");
    } else if (deleteType == GTK_DELETE_PARAGRAPHS) {
        if (!direction)
            client->addPendingEditorCommand("MoveToBeginningOfParagraph");
        else
            client->addPendingEditorCommand("MoveToEndOfParagraph");
    }

    const char* rawCommand = gtkDeleteCommands[deleteType][direction];
    if (!rawCommand)
      return;

    for (int i = 0; i < abs(count); i++)
        client->addPendingEditorCommand(rawCommand);
}

static const char* const gtkMoveCommands[][4] = {
    { "MoveBackward",                                   "MoveForward",
      "MoveBackwardAndModifySelection",                 "MoveForwardAndModifySelection"             }, // Forward/backward grapheme
    { "MoveLeft",                                       "MoveRight",
      "MoveBackwardAndModifySelection",                 "MoveForwardAndModifySelection"             }, // Left/right grapheme
    { "MoveWordBackward",                               "MoveWordForward",
      "MoveWordBackwardAndModifySelection",             "MoveWordForwardAndModifySelection"         }, // Forward/backward word
    { "MoveUp",                                         "MoveDown",
      "MoveUpAndModifySelection",                       "MoveDownAndModifySelection"                }, // Up/down line
    { "MoveToBeginningOfLine",                          "MoveToEndOfLine",
      "MoveToBeginningOfLineAndModifySelection",        "MoveToEndOfLineAndModifySelection"         }, // Up/down line ends
    { "MoveParagraphForward",                           "MoveParagraphBackward",
      "MoveParagraphForwardAndModifySelection",         "MoveParagraphBackwardAndModifySelection"   }, // Up/down paragraphs
    { "MoveToBeginningOfParagraph",                     "MoveToEndOfParagraph",
      "MoveToBeginningOfParagraphAndModifySelection",   "MoveToEndOfParagraphAndModifySelection"    }, // Up/down paragraph ends.
    { "MovePageUp",                                     "MovePageDown",
      "MovePageUpAndModifySelection",                   "MovePageDownAndModifySelection"            }, // Up/down page
    { "MoveToBeginningOfDocument",                      "MoveToEndOfDocument",
      "MoveToBeginningOfDocumentAndModifySelection",    "MoveToEndOfDocumentAndModifySelection"     }, // Begin/end of buffer
    { 0,                                                0,
      0,                                                0                                           } // Horizontal page movement
};

static void moveCursorCallback(GtkWidget* widget, GtkMovementStep step, gint count, gboolean extendSelection, PageClientImpl* client)
{
    g_signal_stop_emission_by_name(widget, "move-cursor");
    int direction = count > 0 ? 1 : 0;
    if (extendSelection)
        direction += 2;

    if (static_cast<unsigned>(step) >= G_N_ELEMENTS(gtkMoveCommands))
        return;

    const char* rawCommand = gtkMoveCommands[step][direction];
    if (!rawCommand)
        return;

    for (int i = 0; i < abs(count); i++)
        client->addPendingEditorCommand(rawCommand);
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
    { 'B',       CtrlKey,            "ToggleBold"                                  },
    { 'I',       CtrlKey,            "ToggleItalic"                                },
    { VK_ESCAPE, 0,                  "Cancel"                                      },
    { VK_OEM_PERIOD, CtrlKey,        "Cancel"                                      },
    { VK_TAB,    0,                  "InsertTab"                                   },
    { VK_TAB,    ShiftKey,           "InsertBacktab"                               },
    { VK_RETURN, 0,                  "InsertNewline"                               },
    { VK_RETURN, CtrlKey,            "InsertNewline"                               },
    { VK_RETURN, AltKey,             "InsertNewline"                               },
    { VK_RETURN, AltKey | ShiftKey,  "InsertNewline"                               },
};

static const KeyPressEntry keyPressEntries[] = {
    { '\t',   0,                  "InsertTab"                                   },
    { '\t',   ShiftKey,           "InsertBacktab"                               },
    { '\r',   0,                  "InsertNewline"                               },
    { '\r',   CtrlKey,            "InsertNewline"                               },
    { '\r',   AltKey,             "InsertNewline"                               },
    { '\r',   AltKey | ShiftKey,  "InsertNewline"                               },
};

PageClientImpl::PageClientImpl(GtkWidget* viewWidget)
    : m_viewWidget(viewWidget)
    , m_nativeWidget(gtk_text_view_new())
{
    g_signal_connect(m_nativeWidget.get(), "backspace", G_CALLBACK(backspaceCallback), this);
    g_signal_connect(m_nativeWidget.get(), "cut-clipboard", G_CALLBACK(cutClipboardCallback), this);
    g_signal_connect(m_nativeWidget.get(), "copy-clipboard", G_CALLBACK(copyClipboardCallback), this);
    g_signal_connect(m_nativeWidget.get(), "paste-clipboard", G_CALLBACK(pasteClipboardCallback), this);
    g_signal_connect(m_nativeWidget.get(), "select-all", G_CALLBACK(selectAllCallback), this);
    g_signal_connect(m_nativeWidget.get(), "move-cursor", G_CALLBACK(moveCursorCallback), this);
    g_signal_connect(m_nativeWidget.get(), "delete-from-cursor", G_CALLBACK(deleteFromCursorCallback), this);
    g_signal_connect(m_nativeWidget.get(), "toggle-overwrite", G_CALLBACK(toggleOverwriteCallback), this);
    g_signal_connect(m_nativeWidget.get(), "popup-menu", G_CALLBACK(popupMenuCallback), this);
    g_signal_connect(m_nativeWidget.get(), "show-help", G_CALLBACK(showHelpCallback), this);
}

PageClientImpl::~PageClientImpl()
{
}

void PageClientImpl::getEditorCommandsForKeyEvent(const NativeWebKeyboardEvent& event, Vector<WTF::String>& commandList)
{
    m_pendingEditorCommands.clear();

#ifdef GTK_API_VERSION_2
    gtk_bindings_activate_event(GTK_OBJECT(m_nativeWidget.get()), const_cast<GdkEventKey*>(&event.nativeEvent()->key));
#else
    gtk_bindings_activate_event(G_OBJECT(m_nativeWidget.get()), const_cast<GdkEventKey*>(&event.nativeEvent()->key));
#endif

    if (m_pendingEditorCommands.isEmpty()) {
        commandList.append(m_pendingEditorCommands);
        return;
    }

    DEFINE_STATIC_LOCAL(IntConstCharHashMap, keyDownCommandsMap, ());
    DEFINE_STATIC_LOCAL(IntConstCharHashMap, keyPressCommandsMap, ());

    if (keyDownCommandsMap.isEmpty()) {
        for (unsigned i = 0; i < G_N_ELEMENTS(keyDownEntries); i++)
            keyDownCommandsMap.set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey, keyDownEntries[i].name);

        for (unsigned i = 0; i < G_N_ELEMENTS(keyPressEntries); i++)
            keyPressCommandsMap.set(keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode, keyPressEntries[i].name);
    }

    unsigned modifiers = 0;
    if (event.shiftKey())
        modifiers |= ShiftKey;
    if (event.altKey())
        modifiers |= AltKey;
    if (event.controlKey())
        modifiers |= CtrlKey;

    // For keypress events, we want charCode(), but keyCode() does that.
    int mapKey = modifiers << 16 | event.nativeVirtualKeyCode();
    if (mapKey) {
        HashMap<int, const char*>* commandMap = event.type() == WebEvent::KeyDown ?
            &keyDownCommandsMap : &keyPressCommandsMap;
        if (const char* commandString = commandMap->get(mapKey))
            m_pendingEditorCommands.append(commandString);
    }

    commandList.append(m_pendingEditorCommands);
}

// PageClient's pure virtual functions
PassOwnPtr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy()
{
    WebKitWebViewBase* view = WEBKIT_WEB_VIEW_BASE(m_viewWidget);
    return ChunkedUpdateDrawingAreaProxy::create(view, webkitWebViewBaseGetPage(view));
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::IntRect&)
{
    notImplemented();
}

void PageClientImpl::displayView()
{
    notImplemented();
}

void PageClientImpl::scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset)
{
    notImplemented();
}

WebCore::IntSize PageClientImpl::viewSize()
{
    GtkAllocation allocation;
    gtk_widget_get_allocation(m_viewWidget, &allocation);
    return IntSize(allocation.width, allocation.height);
}

bool PageClientImpl::isViewWindowActive()
{
    notImplemented();
    return true;
}

bool PageClientImpl::isViewFocused()
{
    notImplemented();
    return true;
}

bool PageClientImpl::isViewVisible()
{
    notImplemented();
    return true;
}

bool PageClientImpl::isViewInWindow()
{
    notImplemented();
    return true;
}

void PageClientImpl::PageClientImpl::processDidCrash()
{
    notImplemented();
}

void PageClientImpl::didRelaunchProcess()
{
    notImplemented();
}

void PageClientImpl::takeFocus(bool)
{
    notImplemented();
}

void PageClientImpl::toolTipChanged(const String&, const String&)
{
    notImplemented();
}

void PageClientImpl::setCursor(const Cursor& cursor)
{
    // [GTK] Widget::setCursor() gets called frequently
    // http://bugs.webkit.org/show_bug.cgi?id=16388
    // Setting the cursor may be an expensive operation in some backends,
    // so don't re-set the cursor if it's already set to the target value.
    GdkWindow* window = gtk_widget_get_window(m_viewWidget);
    GdkCursor* currentCursor = gdk_window_get_cursor(window);
    GdkCursor* newCursor = cursor.platformCursor().get();
    if (currentCursor != newCursor)
        gdk_window_set_cursor(window, newCursor);
}

void PageClientImpl::setViewportArguments(const WebCore::ViewportArguments&)
{
    notImplemented();
}

void PageClientImpl::registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo)
{
    notImplemented();
}

void PageClientImpl::clearAllEditCommands()
{
    notImplemented();
}

bool PageClientImpl::canUndoRedo(WebPageProxy::UndoOrRedo)
{
    notImplemented();
    return false;
}

void PageClientImpl::executeUndoRedo(WebPageProxy::UndoOrRedo)
{
    notImplemented();
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

IntRect PageClientImpl::windowToScreen(const IntRect& rect)
{
    notImplemented();
    return IntRect();
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled)
{
    notImplemented();
}

void PageClientImpl::didNotHandleKeyEvent(const NativeWebKeyboardEvent& event)
{
    notImplemented();
}

PassRefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy*)
{
    notImplemented();
    return 0;
}

PassRefPtr<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy*)
{
    notImplemented();
    return 0;
}

void PageClientImpl::setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut)
{
    notImplemented();
}

#if USE(ACCELERATED_COMPOSITING)
void PageClientImpl::pageDidEnterAcceleratedCompositing()
{
    notImplemented();
}

void PageClientImpl::pageDidLeaveAcceleratedCompositing()
{
    notImplemented();
}
#endif // USE(ACCELERATED_COMPOSITING)

void PageClientImpl::didCommitLoadForMainFrame(bool useCustomRepresentation)
{
}

void PageClientImpl::didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference&)
{
}

double PageClientImpl::customRepresentationZoomFactor()
{
    notImplemented();
    return 0;
}

void PageClientImpl::setCustomRepresentationZoomFactor(double)
{
    notImplemented();
}

void PageClientImpl::pageClosed()
{
    notImplemented();
}

void PageClientImpl::didChangeScrollbarsForMainFrame() const
{
}

void PageClientImpl::flashBackingStoreUpdates(const Vector<IntRect>&)
{
    notImplemented();
}

void PageClientImpl::findStringInCustomRepresentation(const String&, FindOptions, unsigned)
{
    notImplemented();
}

void PageClientImpl::countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned)
{
    notImplemented();
}

} // namespace WebKit
