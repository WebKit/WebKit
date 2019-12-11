/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "WebSelectionServiceController.h"

#if ENABLE(SERVICE_CONTROLS)

#import "WebViewInternal.h"
#import <WebCore/FrameSelection.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/Range.h>
#import <pal/spi/mac/NSSharingServiceSPI.h>

using namespace WebCore;

WebSelectionServiceController::WebSelectionServiceController(WebView *webView) 
    : WebSharingServicePickerClient(webView)
{
}

void WebSelectionServiceController::handleSelectionServiceClick(WebCore::FrameSelection& selection, const Vector<String>& telephoneNumbers, const WebCore::IntPoint& point)
{
    Page* page = [m_webView page];
    if (!page)
        return;

    RefPtr<Range> range = selection.selection().firstRange();
    if (!range)
        return;

    RetainPtr<NSAttributedString> attributedSelection = attributedStringFromRange(*range);
    if (!attributedSelection)
        return;

    NSArray *items = @[ attributedSelection.get() ];

    bool isEditable = selection.selection().isContentEditable();
    
    m_sharingServicePickerController = adoptNS([[WebSharingServicePickerController alloc] initWithItems:items includeEditorServices:isEditable client:this style:NSSharingServicePickerStyleTextSelection]);

    RetainPtr<NSMenu> menu = adoptNS([[m_sharingServicePickerController menu] copy]);

    [menu setShowsStateColumn:YES];

    [menu popUpMenuPositioningItem:nil atLocation:[m_webView convertPoint:point toView:nil] inView:m_webView];
}

static bool hasCompatibleServicesForItems(NSArray *items)
{
    return [NSSharingService sharingServicesForItems:items mask:NSSharingServiceMaskViewer | NSSharingServiceMaskEditor].count;
}

bool WebSelectionServiceController::hasRelevantSelectionServices(bool isTextOnly) const
{
    RetainPtr<NSAttributedString> attributedString = adoptNS([[NSAttributedString alloc] initWithString:@"a"]);

    bool hasSelectionServices = hasCompatibleServicesForItems(@[ attributedString.get() ]);
    if (isTextOnly && hasSelectionServices)
        return true;

    auto attachment = adoptNS([[NSTextAttachment alloc] init]);
    auto image = adoptNS([[NSImage alloc] init]);
    auto cell = adoptNS([[NSTextAttachmentCell alloc] initImageCell:image.get()]);
    [attachment setAttachmentCell:cell.get()];
    NSMutableAttributedString *attributedStringWithRichContent = (NSMutableAttributedString *)[NSMutableAttributedString attributedStringWithAttachment:attachment.get()];
    [attributedStringWithRichContent appendAttributedString:attributedString.get()];

    return hasCompatibleServicesForItems(@[ attributedStringWithRichContent ]);
}

void WebSelectionServiceController::sharingServicePickerWillBeDestroyed(WebSharingServicePickerController &)
{
    m_sharingServicePickerController = nil;
}

#endif
