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

#import "config.h"
#import "ServicesController.h"

#if ENABLE(SERVICE_CONTROLS)

#import "WebContext.h"
#import "WebProcessMessages.h"
#import <WebCore/NSExtensionSPI.h>
#import <WebCore/NSSharingServicePickerSPI.h>
#import <WebCore/NSSharingServiceSPI.h>
#import <wtf/NeverDestroyed.h>

namespace WebKit {

ServicesController& ServicesController::shared()
{
    static NeverDestroyed<ServicesController> sharedController;
    return sharedController;
}

ServicesController::ServicesController()
    : m_refreshQueue(dispatch_queue_create("com.apple.WebKit.ServicesController", DISPATCH_QUEUE_SERIAL))
    , m_hasPendingRefresh(false)
    , m_hasImageServices(false)
    , m_hasSelectionServices(false)
    , m_hasRichContentServices(false)
{
    refreshExistingServices();

#ifdef __LP64__
    auto refreshCallback = [](NSArray *, NSError *) {
        // We coalese refreshes from the notification callbacks because they can come in small batches.
        ServicesController::shared().refreshExistingServices(false);
    };

    auto extensionAttributes = @{ @"NSExtensionPointName" : @"com.apple.services" };
    m_extensionWatcher = [NSExtension beginMatchingExtensionsWithAttributes:extensionAttributes completion:refreshCallback];
    auto uiExtensionAttributes = @{ @"NSExtensionPointName" : @"com.apple.ui-services" };
    m_uiExtensionWatcher = [NSExtension beginMatchingExtensionsWithAttributes:uiExtensionAttributes completion:refreshCallback];
#endif // __LP64__
}

static bool hasCompatibleServicesForItems(NSArray *items)
{
    return [NSSharingService sharingServicesForItems:items mask:NSSharingServiceMaskViewer | NSSharingServiceMaskEditor].count;
}

void ServicesController::refreshExistingServices(bool refreshImmediately)
{
    if (m_hasPendingRefresh)
        return;

    m_hasPendingRefresh = true;

    auto refreshTime = dispatch_time(DISPATCH_TIME_NOW, refreshImmediately ? 0 : (int64_t)(1 * NSEC_PER_SEC));
    dispatch_after(refreshTime, m_refreshQueue, ^{
        static NeverDestroyed<NSImage *> image([[NSImage alloc] init]);
        bool hasImageServices = hasCompatibleServicesForItems(@[ image ]);

        static NeverDestroyed<NSAttributedString *> attributedString([[NSAttributedString alloc] initWithString:@"a"]);
        bool hasSelectionServices = hasCompatibleServicesForItems(@[ attributedString ]);

        static NSAttributedString *attributedStringWithRichContent;
        if (!attributedStringWithRichContent) {
            NSTextAttachment *attachment = [[NSTextAttachment alloc] init];
            NSTextAttachmentCell *cell = [[NSTextAttachmentCell alloc] initImageCell:image.get()];
            [attachment setAttachmentCell:cell];
            NSMutableAttributedString *richString = (NSMutableAttributedString *)[NSMutableAttributedString attributedStringWithAttachment:attachment];
            [richString appendAttributedString: attributedString];
            attributedStringWithRichContent = [richString retain];
        }

        bool hasRichContentServices = hasCompatibleServicesForItems(@[ attributedStringWithRichContent ]);
        
        dispatch_async(dispatch_get_main_queue(), ^{
            bool availableServicesChanged = (hasImageServices != m_hasImageServices) || (hasSelectionServices != m_hasSelectionServices) || (hasRichContentServices != m_hasRichContentServices);

            m_hasSelectionServices = hasSelectionServices;
            m_hasImageServices = hasImageServices;
            m_hasRichContentServices = hasRichContentServices;

            if (availableServicesChanged) {
                for (auto& context : WebContext::allContexts())
                    context->sendToAllProcesses(Messages::WebProcess::SetEnabledServices(m_hasImageServices, m_hasSelectionServices, m_hasRichContentServices));
            }

            m_hasPendingRefresh = false;
        });
    });
}

} // namespace WebKit

#endif // ENABLE(SERVICE_CONTROLS)
