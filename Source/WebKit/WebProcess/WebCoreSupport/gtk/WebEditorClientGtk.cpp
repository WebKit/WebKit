/*
 *  Copyright (C) 2011 Igalia S.L.
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

#include <WebCore/Document.h>
#include <WebCore/Editor.h>
#include <WebCore/EventNames.h>
#include <WebCore/Frame.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/PagePasteboardContext.h>
#include <WebCore/Pasteboard.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/TextIterator.h>
#include <WebCore/markup.h>
#include <WebPage.h>
#include <wtf/Variant.h>
#include <wtf/glib/GRefPtr.h>

namespace WebKit {
using namespace WebCore;

bool WebEditorClient::handleGtkEditorCommand(Frame& frame, const String& command, bool allowTextInsertion)
{
    if (command == "GtkInsertEmoji"_s) {
        if (!allowTextInsertion)
            return false;
        m_page->showEmojiPicker(frame);
        return true;
    }

    return false;
}

bool WebEditorClient::executePendingEditorCommands(Frame& frame, const Vector<WTF::String>& pendingEditorCommands, bool allowTextInsertion)
{
    Vector<Variant<Editor::Command, String>> commands;
    for (auto& commandString : pendingEditorCommands) {
        if (commandString.startsWith("Gtk"))
            commands.append(commandString);
        else {
            Editor::Command command = frame.editor().command(commandString);
            if (command.isTextInsertion() && !allowTextInsertion)
                return false;

            commands.append(WTFMove(command));
        }
    }

    for (auto& commandVariant : commands) {
        if (WTF::holds_alternative<String>(commandVariant)) {
            if (!handleGtkEditorCommand(frame, WTF::get<String>(commandVariant), allowTextInsertion))
                return false;
        } else {
            auto& command = WTF::get<Editor::Command>(commandVariant);
            if (!command.execute())
                return false;
        }
    }

    return true;
}

void WebEditorClient::handleKeyboardEvent(KeyboardEvent& event)
{
    auto* platformEvent = event.underlyingPlatformEvent();
    if (!platformEvent)
        return;

    // If this was an IME event don't do anything.
    if (platformEvent->handledByInputMethod())
        return;

    ASSERT(event.target());
    auto* frame = downcast<Node>(event.target())->document().frame();
    ASSERT(frame);

    const Vector<String> pendingEditorCommands = platformEvent->commands();
    if (!pendingEditorCommands.isEmpty()) {

        // During RawKeyDown events if an editor command will insert text, defer
        // the insertion until the keypress event. We want keydown to bubble up
        // through the DOM first.
        if (platformEvent->type() == PlatformEvent::RawKeyDown) {
            if (executePendingEditorCommands(*frame, pendingEditorCommands, false))
                event.setDefaultHandled();

            return;
        }

        // Only allow text insertion commands if the current node is editable.
        if (executePendingEditorCommands(*frame, pendingEditorCommands, frame->editor().canEdit())) {
            event.setDefaultHandled();
            return;
        }
    }

    // Don't allow text insertion for nodes that cannot edit.
    if (!frame->editor().canEdit())
        return;

    // This is just a normal text insertion, so wait to execute the insertion
    // until a keypress event happens. This will ensure that the insertion will not
    // be reflected in the contents of the field until the keyup DOM event.
    if (event.type() != eventNames().keypressEvent)
        return;

    // Don't insert null or control characters as they can result in unexpected behaviour
    if (event.charCode() < ' ')
        return;

    // Don't insert anything if a modifier is pressed
    if (platformEvent->controlKey() || platformEvent->altKey())
        return;

    if (frame->editor().insertText(platformEvent->text(), &event))
        event.setDefaultHandled();
}

void WebEditorClient::updateGlobalSelection(Frame* frame)
{
    if (!frame->selection().isRange())
        return;
    auto range = frame->selection().selection().toNormalizedRange();
    if (!range)
        return;

    PasteboardWebContent pasteboardContent;
    pasteboardContent.canSmartCopyOrDelete = false;
    pasteboardContent.text = plainText(*range);
    pasteboardContent.markup = serializePreservingVisualAppearance(frame->selection().selection(), ResolveURLs::YesExcludingLocalFileURLsForPrivacy);
    Pasteboard::createForGlobalSelection(PagePasteboardContext::create(frame->pageID()))->write(pasteboardContent);
}

bool WebEditorClient::shouldShowUnicodeMenu()
{
    return true;
}

}
