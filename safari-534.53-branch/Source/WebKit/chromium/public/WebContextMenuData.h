/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebContextMenuData_h
#define WebContextMenuData_h

#include "WebHistoryItem.h"
#include "WebMenuItemInfo.h"
#include "WebNode.h"
#include "WebPoint.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebVector.h"

#define WEBCONTEXT_MEDIATYPEFILE_DEFINED

namespace WebKit {

// This struct is passed to WebViewClient::ShowContextMenu.
struct WebContextMenuData {
    enum MediaType {
        // No special node is in context.
        MediaTypeNone,
        // An image node is selected.
        MediaTypeImage,
        // A video node is selected.
        MediaTypeVideo,
        // An audio node is selected.
        MediaTypeAudio,
        // A file node is selected.
        MediaTypeFile,
        // A plugin node is selected.
        MediaTypePlugin,
    };
    // The type of media the context menu is being invoked on.
    MediaType mediaType;

    // The x and y position of the mouse pointer (relative to the webview).
    WebPoint mousePosition;

    // The absolute URL of the link that is in context.
    WebURL linkURL;

    // The absolute URL of the image/video/audio that is in context.
    WebURL srcURL;

    // Whether the image in context has been blocked.
    bool isImageBlocked;

    // The absolute URL of the page in context.
    WebURL pageURL;

    // The absolute URL of the subframe in context.
    WebURL frameURL;

    // The encoding for the frame in context.
    WebString frameEncoding;

    // History state of the subframe in context.
    WebHistoryItem frameHistoryItem;

    enum MediaFlags {
        MediaNone = 0x0,
        MediaInError = 0x1,
        MediaPaused = 0x2,
        MediaMuted = 0x4,
        MediaLoop = 0x8,
        MediaCanSave = 0x10,
        MediaHasAudio = 0x20,
        MediaHasVideo = 0x40,
        MediaControlRootElement = 0x80,
        MediaCanPrint = 0x100,
    };

    // Extra attributes describing media elements.
    int mediaFlags;

    // The raw text of the selection in context.
    WebString selectedText;

    // Whether spell checking is enabled.
    bool isSpellCheckingEnabled;

    // The editable (possibily) misspelled word.
    WebString misspelledWord;

    // If misspelledWord is not empty, holds suggestions from the dictionary.
    WebVector<WebString> dictionarySuggestions;

    // Whether context is editable.
    bool isEditable;

    enum CheckableMenuItemFlags {
        CheckableMenuItemDisabled = 0x0,
        CheckableMenuItemEnabled = 0x1,
        CheckableMenuItemChecked = 0x2,
    };

    // Writing direction menu items - values are unions of
    // CheckableMenuItemFlags.
    // Currently only used on OS X.
    int writingDirectionDefault;
    int writingDirectionLeftToRight;
    int writingDirectionRightToLeft;

    enum EditFlags {
        CanDoNone = 0x0,
        CanUndo = 0x1,
        CanRedo = 0x2,
        CanCut = 0x4,
        CanCopy = 0x8,
        CanPaste = 0x10,
        CanDelete = 0x20,
        CanSelectAll = 0x40,
        CanTranslate = 0x80,
    };

    // Which edit operations are available in the context.
    int editFlags;

    // Security information for the context.
    WebCString securityInfo;

    // Custom context menu items provided by the WebCore internals.
    WebVector<WebMenuItemInfo> customItems;

    // The node that was clicked.
    WebNode node;

    WebContextMenuData()
        : mediaType(MediaTypeNone)
        , isImageBlocked(false)
        , mediaFlags(MediaNone)
        , isSpellCheckingEnabled(false)
        , isEditable(false)
        , writingDirectionDefault(CheckableMenuItemDisabled)
        , writingDirectionLeftToRight(CheckableMenuItemEnabled)
        , writingDirectionRightToLeft(CheckableMenuItemEnabled)
        , editFlags(0) { }
};

} // namespace WebKit

#endif
