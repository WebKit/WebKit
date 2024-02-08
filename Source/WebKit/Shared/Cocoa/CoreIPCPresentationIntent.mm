/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CoreIPCPresentationIntent.h"

#if PLATFORM(COCOA)

#import <Foundation/Foundation.h>
#import <wtf/RetainPtr.h>

namespace WebKit {

CoreIPCPresentationIntent::CoreIPCPresentationIntent(NSPresentationIntent *intent)
    : m_intentKind(intent.intentKind)
    , m_identity(intent.identity)
{
    if (intent.parentIntent)
        m_parentIntent = makeUnique<CoreIPCPresentationIntent>(intent.parentIntent);

    switch (m_intentKind) {
    case NSPresentationIntentKindHeader:
        m_headerLevel = intent.headerLevel;
        break;
    case NSPresentationIntentKindListItem:
        m_ordinal = intent.ordinal;
        break;
    case NSPresentationIntentKindCodeBlock:
        m_languageHint = { intent.languageHint };
        break;
    case NSPresentationIntentKindTable:
        for (NSNumber *alignment in intent.columnAlignments)
            m_columnAlignments.append(alignment.unsignedIntegerValue);
        m_columnCount = intent.columnCount;
        break;
    case NSPresentationIntentKindTableRow:
        m_row = intent.row;
        break;
    case NSPresentationIntentKindTableCell:
        m_column = intent.column;
        break;
    case NSPresentationIntentKindParagraph:
    case NSPresentationIntentKindOrderedList:
    case NSPresentationIntentKindUnorderedList:
    case NSPresentationIntentKindBlockQuote:
    case NSPresentationIntentKindThematicBreak:
    case NSPresentationIntentKindTableHeaderRow:
        break;
    }
}

RetainPtr<id> CoreIPCPresentationIntent::toID() const
{
    auto parent = (m_parentIntent ? m_parentIntent->toID(): nullptr);
    switch (m_intentKind) {
    case NSPresentationIntentKindParagraph:
        return [NSPresentationIntent paragraphIntentWithIdentity:m_identity nestedInsideIntent:parent.get()];
    case NSPresentationIntentKindHeader:
        return [NSPresentationIntent headerIntentWithIdentity:m_identity level:m_headerLevel nestedInsideIntent:parent.get()];
    case NSPresentationIntentKindOrderedList:
        return [NSPresentationIntent orderedListIntentWithIdentity:m_identity nestedInsideIntent:parent.get()];
    case NSPresentationIntentKindUnorderedList:
        return [NSPresentationIntent unorderedListIntentWithIdentity:m_identity nestedInsideIntent:parent.get()];
    case NSPresentationIntentKindListItem:
        return [NSPresentationIntent listItemIntentWithIdentity:m_identity ordinal:m_ordinal nestedInsideIntent:parent.get()];
    case NSPresentationIntentKindCodeBlock:
        return [NSPresentationIntent codeBlockIntentWithIdentity:m_identity languageHint:m_languageHint nestedInsideIntent:parent.get()];
    case NSPresentationIntentKindBlockQuote:
        return [NSPresentationIntent blockQuoteIntentWithIdentity:m_identity nestedInsideIntent:parent.get()];
    case NSPresentationIntentKindThematicBreak:
        return [NSPresentationIntent thematicBreakIntentWithIdentity:m_identity nestedInsideIntent:parent.get()];
    case NSPresentationIntentKindTable: {
        auto columnAlignments = adoptNS([[NSMutableArray alloc] initWithCapacity:m_columnAlignments.size()]);
        for (int64_t alignment : m_columnAlignments)
            [columnAlignments.get() addObject:@(alignment)];
        return [NSPresentationIntent tableIntentWithIdentity:m_identity columnCount:m_columnCount alignments:columnAlignments.get() nestedInsideIntent:parent.get()];
    }
    case NSPresentationIntentKindTableHeaderRow:
        return [NSPresentationIntent tableHeaderRowIntentWithIdentity:m_identity nestedInsideIntent:parent.get()];
    case NSPresentationIntentKindTableRow:
        return [NSPresentationIntent tableRowIntentWithIdentity:m_identity row:m_row nestedInsideIntent:parent.get()];
    case NSPresentationIntentKindTableCell:
        return [NSPresentationIntent tableCellIntentWithIdentity:m_identity column:m_column nestedInsideIntent:parent.get()];
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
