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

#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import <pal/spi/cocoa/NSExtensionSPI.h>
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#import <pal/spi/mac/NSSharingServiceSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>

namespace WebKit {

ServicesController& ServicesController::singleton()
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

    auto refreshCallback = [this](NSArray *, NSError *) {
        // We coalese refreshes from the notification callbacks because they can come in small batches.
        refreshExistingServices(false);
    };

    auto extensionAttributes = @{ @"NSExtensionPointName" : @"com.apple.services" };
    m_extensionWatcher = [NSExtension beginMatchingExtensionsWithAttributes:extensionAttributes completion:refreshCallback];
    auto uiExtensionAttributes = @{ @"NSExtensionPointName" : @"com.apple.ui-services" };
    m_uiExtensionWatcher = [NSExtension beginMatchingExtensionsWithAttributes:uiExtensionAttributes completion:refreshCallback];
}

static void hasCompatibleServicesForItems(dispatch_group_t group, NSArray *items, WTF::Function<void(bool)>&& completionHandler)
{
    NSSharingServiceMask servicesMask = NSSharingServiceMaskViewer | NSSharingServiceMaskEditor;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    if ([NSSharingService respondsToSelector:@selector(getSharingServicesForItems:mask:completion:)]) {
        dispatch_group_enter(group);
        [NSSharingService getSharingServicesForItems:items mask:servicesMask completion:makeBlockPtr([completionHandler = WTFMove(completionHandler), group](NSArray *services) {
            completionHandler(services.count);
            dispatch_group_leave(group);
        }).get()];
        return;
    }
#else
    UNUSED_PARAM(group);
#endif
    
    completionHandler([NSSharingService sharingServicesForItems:items mask:servicesMask].count);
}

void ServicesController::refreshExistingServices(bool refreshImmediately)
{
    if (m_hasPendingRefresh)
        return;

    m_hasPendingRefresh = true;

    auto refreshTime = dispatch_time(DISPATCH_TIME_NOW, refreshImmediately ? 0 : (int64_t)(1 * NSEC_PER_SEC));
    dispatch_after(refreshTime, m_refreshQueue, ^{
        auto serviceLookupGroup = adoptOSObject(dispatch_group_create());

        static NSImage *image { [[NSImage alloc] init] };
        hasCompatibleServicesForItems(serviceLookupGroup.get(), @[ image ], [this] (bool hasServices) {
            m_hasImageServices = hasServices;
        });

        static NSAttributedString *attributedString { [[NSAttributedString alloc] initWithString:@"a"] };
        hasCompatibleServicesForItems(serviceLookupGroup.get(), @[ attributedString ], [this] (bool hasServices) {
            m_hasSelectionServices = hasServices;
        });

        static NSAttributedString *attributedStringWithRichContent = [] {
            NSMutableAttributedString *richString;
            dispatch_sync(dispatch_get_main_queue(), [&richString] {
                auto attachment = adoptNS([[NSTextAttachment alloc] init]);
                auto cell = adoptNS([[NSTextAttachmentCell alloc] initImageCell:image]);
                [attachment setAttachmentCell:cell.get()];
                richString = [[NSAttributedString attributedStringWithAttachment:attachment.get()] mutableCopy];
                [richString appendAttributedString:attributedString];
            });
            return richString;
        }();

        hasCompatibleServicesForItems(serviceLookupGroup.get(), @[ attributedStringWithRichContent ], [this] (bool hasServices) {
            m_hasRichContentServices = hasServices;
        });

        dispatch_group_notify(serviceLookupGroup.get(), dispatch_get_main_queue(), makeBlockPtr([this] {
            bool availableServicesChanged = (m_lastSentHasImageServices != m_hasImageServices) || (m_lastSentHasSelectionServices != m_hasSelectionServices) || (m_lastSentHasRichContentServices != m_hasRichContentServices);

            m_lastSentHasSelectionServices = m_hasSelectionServices;
            m_lastSentHasImageServices = m_hasImageServices;
            m_lastSentHasRichContentServices = m_hasRichContentServices;

            if (availableServicesChanged) {
                for (auto& processPool : WebProcessPool::allProcessPools())
                    processPool->sendToAllProcesses(Messages::WebProcess::SetEnabledServices(m_hasImageServices, m_hasSelectionServices, m_hasRichContentServices));
            }

            m_hasPendingRefresh = false;
        }).get());
    });
}

} // namespace WebKit

#endif // ENABLE(SERVICE_CONTROLS)
