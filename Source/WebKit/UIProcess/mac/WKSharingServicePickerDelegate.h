/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#pragma once

#import <wtf/Platform.h>

#if ENABLE(SERVICE_CONTROLS)

#import "WebContextMenuProxyMac.h"
#import <AppKit/AppKit.h>
#import <pal/spi/mac/NSSharingServiceSPI.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

NS_SWIFT_UI_ACTOR
@interface WKSharingServicePickerDelegate : NSObject <NSSharingServiceDelegate, NSSharingServicePickerDelegate>

@property (class, nonatomic, readonly) WKSharingServicePickerDelegate *sharedSharingServicePickerDelegate NS_SWIFT_NAME(shared);

@property (nonatomic, strong, nullable) NSSharingServicePicker *picker;
@property (nonatomic) BOOL filtersEditingServices;
@property (nonatomic) BOOL handlesEditingReplacement;
@property (nonatomic) NSRect sourceFrame;
@property (nonatomic, copy, nullable) NSString *attachmentID;

- (void)setMenuProxy:(WebKit::WebContextMenuProxyMac *)menuProxy;
- (void)removeBackground;

// From the private NSSharingServicePickerDelegateNewAPI protocol, which the public SDK doesn't declare.
- (NSArray<NSSharingService *> *)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker sharingServicesForItems:(NSArray *)items mask:(NSSharingServiceMask)mask proposedSharingServices:(NSArray<NSSharingService *> *)proposedServices;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // ENABLE(SERVICE_CONTROLS)
