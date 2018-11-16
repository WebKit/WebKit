/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "Editor.h"

#if USE(LIBWPE)

#include "DocumentFragment.h"
#include "Frame.h"
#include "NotImplemented.h"
#include "Pasteboard.h"
#include "Settings.h"
#include "markup.h"

namespace WebCore {

static RefPtr<DocumentFragment> createFragmentFromPasteboardData(Pasteboard& pasteboard, Frame& frame, Range& range, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    Vector<String> types = pasteboard.typesForLegacyUnsafeBindings();
    if (types.isEmpty())
        return nullptr;

    if (types.contains("text/html;charset=utf-8") && frame.document()) {
        String markup = pasteboard.readString("text/html;charset=utf-8");
        if (RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(*frame.document(), markup, emptyString(), DisallowScriptingAndPluginContent))
            return fragment;
    }

    if (!allowPlainText)
        return nullptr;

    if (types.contains("text/plain;charset=utf-8")) {
        chosePlainText = true;
        if (RefPtr<DocumentFragment> fragment = createFragmentFromText(range, pasteboard.readString("text/plain;charset=utf-8")))
            return fragment;
    }

    return nullptr;
}

void Editor::writeSelectionToPasteboard(Pasteboard& pasteboard)
{
    PasteboardWebContent pasteboardContent;
    pasteboardContent.text = selectedTextForDataTransfer();
    pasteboardContent.markup = serializePreservingVisualAppearance(m_frame.selection().selection(), ResolveURLs::YesExcludingLocalFileURLsForPrivacy,
        m_frame.settings().selectionAcrossShadowBoundariesEnabled() ? SerializeComposedTree::Yes : SerializeComposedTree::No);
    pasteboard.write(pasteboardContent);
}

void Editor::writeImageToPasteboard(Pasteboard&, Element&, const URL&, const String&)
{
    notImplemented();
}

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, OptionSet<PasteOption> options)
{
    RefPtr<Range> range = selectedRange();
    if (!range)
        return;

    bool chosePlainText;
    RefPtr<DocumentFragment> fragment = createFragmentFromPasteboardData(*pasteboard, m_frame, *range, options.contains(PasteOption::AllowPlainText), chosePlainText);

    if (fragment && options.contains(PasteOption::AsQuotation))
        quoteFragmentForPasting(*fragment);

    if (fragment && shouldInsertFragment(*fragment, range.get(), EditorInsertAction::Pasted))
        pasteAsFragment(*fragment, canSmartReplaceWithPasteboard(*pasteboard), chosePlainText, options.contains(PasteOption::IgnoreMailBlockquote) ? MailBlockquoteHandling::IgnoreBlockquote : MailBlockquoteHandling::RespectBlockquote);
}

} // namespace WebCore

#endif // USE(LIBWPE)
