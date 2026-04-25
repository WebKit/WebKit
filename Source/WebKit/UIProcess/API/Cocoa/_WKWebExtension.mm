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
#import "_WKWebExtension.h"

#import "_WKWebExtensionAction.h"
#import "_WKWebExtensionCommand.h"
#import "_WKWebExtensionContext.h"
#import "_WKWebExtensionController.h"
#import "_WKWebExtensionControllerConfiguration.h"
#import "_WKWebExtensionDataRecord.h"
#import "_WKWebExtensionMatchPattern.h"
#import "_WKWebExtensionMessagePort.h"
#import "_WKWebExtensionTab.h"
#import "_WKWebExtensionTabCreationOptions.h"
#import "_WKWebExtensionWindow.h"
#import "_WKWebExtensionWindowCreationOptions.h"

#undef _WKWebExtensionTab
#undef _WKWebExtensionWindow
@interface _WKWebExtensionStaging : NSObject <_WKWebExtensionTab, _WKWebExtensionWindow>
@end

@implementation _WKWebExtensionStaging
@end

NSErrorDomain const _WKWebExtensionErrorDomain = @"WKWebExtensionErrorDomain";

NSNotificationName const _WKWebExtensionErrorsWereUpdatedNotification = @"WKWebExtensionContextErrorsDidUpdate";

#undef _WKWebExtension
@implementation _WKWebExtension
@end

NSNotificationName const _WKWebExtensionActionPropertiesDidChangeNotification = @"WKWebExtensionActionPropertiesDidChange";

#undef _WKWebExtensionAction
@implementation _WKWebExtensionAction
@end

#undef _WKWebExtensionCommand
@implementation _WKWebExtensionCommand
@end

NSErrorDomain const _WKWebExtensionContextErrorDomain = @"WKWebExtensionContextErrorDomain";

NSNotificationName const _WKWebExtensionContextPermissionsWereGrantedNotification = @"WKWebExtensionContextPermissionsWereGranted";
NSNotificationName const _WKWebExtensionContextPermissionsWereDeniedNotification = @"WKWebExtensionContextPermissionsWereDenied";
NSNotificationName const _WKWebExtensionContextGrantedPermissionsWereRemovedNotification = @"WKWebExtensionContextGrantedPermissionsWereRemoved";
NSNotificationName const _WKWebExtensionContextDeniedPermissionsWereRemovedNotification = @"WKWebExtensionContextDeniedPermissionsWereRemoved";

NSNotificationName const _WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification = @"WKWebExtensionContextPermissionMatchPatternsWereGranted";
NSNotificationName const _WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification = @"WKWebExtensionContextPermissionMatchPatternsWereDenied";
NSNotificationName const _WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification = @"WKWebExtensionContextGrantedPermissionMatchPatternsWereRemoved";
NSNotificationName const _WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification = @"WKWebExtensionContextDeniedPermissionMatchPatternsWereRemoved";

NSString * const _WKWebExtensionContextNotificationUserInfoKeyPermissions = @"permissions";
NSString * const _WKWebExtensionContextNotificationUserInfoKeyMatchPatterns = @"matchPatterns";

#undef _WKWebExtensionContext
@implementation _WKWebExtensionContext
@end

#undef _WKWebExtensionController
@implementation _WKWebExtensionController
@end

#undef _WKWebExtensionControllerConfiguration
@implementation _WKWebExtensionControllerConfiguration
@end

NSErrorDomain const _WKWebExtensionDataRecordErrorDomain = @"WKWebExtensionDataRecordErrorDomain";

#undef _WKWebExtensionDataRecord
@implementation _WKWebExtensionDataRecord
@end

NSErrorDomain const _WKWebExtensionMatchPatternErrorDomain = @"WKWebExtensionMatchPatternErrorDomain";

#undef _WKWebExtensionMatchPattern
@implementation _WKWebExtensionMatchPattern
@end

NSErrorDomain const _WKWebExtensionMessagePortErrorDomain = @"WKWebExtensionMessagePortErrorDomain";

#undef _WKWebExtensionMessagePort
@implementation _WKWebExtensionMessagePort
@end

#undef _WKWebExtensionTabCreationOptions
@implementation _WKWebExtensionTabCreationOptions
@end

#undef _WKWebExtensionWindowCreationOptions
@implementation _WKWebExtensionWindowCreationOptions
@end

