/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if JSC_OBJC_API_ENABLED

#import <JavaScriptCore/JavaScriptCore.h>

@interface JSValue(JSPrivate)

/*!
 @method
 @abstract Create a new promise object using the provided executor callback.
 @param callback A callback block invoked while the promise object is
 being initialized. The resolve and reject parameters are functions that
 can be called to notify any pending reactions about the state of the
 new promise object.
 @param context The JSContext to which the resulting JSValue belongs.
 @result The JSValue representing a new promise JavaScript object.
 @discussion This method is equivalent to calling the Promise constructor in JavaScript.
 the resolve and reject callbacks each normally take a single value, which they
 forward to all relevent pending reactions. While inside the executor callback context will act
 as if it were in any other callback, except calleeFunction will be <code>nil</code>. This also means
 means the new promise object may be accessed via <code>[context thisValue]</code>.
 */
+ (JSValue *)valueWithNewPromiseInContext:(JSContext *)context fromExecutor:(void (^)(JSValue *resolve, JSValue *reject))callback JSC_API_AVAILABLE(macosx(JSC_MAC_TBA), ios(JSC_IOS_TBA));

/*!
 @method
 @abstract Create a new resolved promise object with the provided value.
 @param result The result value to be passed to any reactions.
 @param context The JSContext to which the resulting JSValue belongs.
 @result The JSValue representing a new promise JavaScript object.
 @discussion This method is equivalent to calling <code>[JSValue valueWithNewPromiseFromExecutor:^(JSValue *resolve, JSValue *reject) { [resolve callWithArguments:@[result]]; } inContext:context]</code>
 */
+ (JSValue *)valueWithNewPromiseResolvedWithResult:(id)result inContext:(JSContext *)context JSC_API_AVAILABLE(macosx(JSC_MAC_TBA), ios(JSC_IOS_TBA));

/*!
 @method
 @abstract Create a new rejected promise object with the provided value.
 @param reason The result value to be passed to any reactions.
 @param context The JSContext to which the resulting JSValue belongs.
 @result The JSValue representing a new promise JavaScript object.
 @discussion This method is equivalent to calling <code>[JSValue valueWithNewPromiseFromExecutor:^(JSValue *resolve, JSValue *reject) { [reject callWithArguments:@[reason]]; } inContext:context]</code>
 */
+ (JSValue *)valueWithNewPromiseRejectedWithReason:(id)reason inContext:(JSContext *)context JSC_API_AVAILABLE(macosx(JSC_MAC_TBA), ios(JSC_IOS_TBA));

@end

#endif // JSC_OBJC_API_ENABLED
