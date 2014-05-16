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
#import <wtf/NeverDestroyed.h>

#if __has_include(<AppKit/NSSharingService_Private.h>)
#import <AppKit/NSSharingService_Private.h>
#else
typedef enum {
    NSSharingServicePickerStyleMenu = 0,
    NSSharingServicePickerStyleRollover = 1,
    NSSharingServicePickerStyleTextSelection = 2,
    NSSharingServicePickerStyleDataDetector = 3
} NSSharingServicePickerStyle;

@interface NSSharingServicePicker (Details)
@property NSSharingServicePickerStyle style;
- (NSMenu *)menu;
@end
#endif

namespace WebKit {

ServicesController& ServicesController::shared()
{
    static NeverDestroyed<ServicesController> sharedController;
    return sharedController;
}

ServicesController::ServicesController()
    : m_refreshQueue(dispatch_queue_create("com.apple.WebKit.ServicesController", DISPATCH_QUEUE_SERIAL))
    , m_isRefreshing(false)
    , m_hasImageServices(false)
    , m_hasSelectionServices(false)
{
    refreshExistingServices();
}

void ServicesController::refreshExistingServices(WebContext* context)
{
    ASSERT_ARG(context, context);
    ASSERT([NSThread isMainThread]);

    m_contextsToNotify.add(context);
    refreshExistingServices();
}

void ServicesController::refreshExistingServices()
{
    if (m_isRefreshing)
        return;

    m_isRefreshing = true;
    dispatch_async(m_refreshQueue, ^{
        static NeverDestroyed<NSImage *> image([[NSImage alloc] init]);
        RetainPtr<NSSharingServicePicker>  picker = adoptNS([[NSSharingServicePicker alloc] initWithItems:@[ image ]]);
        [picker setStyle:NSSharingServicePickerStyleRollover];

        bool hasImageServices = picker.get().menu;

        static NeverDestroyed<NSAttributedString *> attributedString([[NSAttributedString alloc] initWithString:@"a"]);
        picker = adoptNS([[NSSharingServicePicker alloc] initWithItems:@[ attributedString ]]);
        [picker setStyle:NSSharingServicePickerStyleTextSelection];

        bool hasSelectionServices = picker.get().menu;

        dispatch_async(dispatch_get_main_queue(), ^{
            bool notifyContexts = (hasImageServices != m_hasImageServices) || (hasSelectionServices != m_hasSelectionServices);
            m_hasSelectionServices = hasSelectionServices;
            m_hasImageServices = hasImageServices;

            if (notifyContexts) {
                for (const RefPtr<WebContext>& context : m_contextsToNotify)
                    context->sendToAllProcesses(Messages::WebProcess::SetEnabledServices(m_hasImageServices, m_hasSelectionServices));
            }

            m_contextsToNotify.clear();

            m_isRefreshing = false;
        });
    });
}

} // namespace WebKit

#endif // ENABLE(SERVICE_CONTROLS)
