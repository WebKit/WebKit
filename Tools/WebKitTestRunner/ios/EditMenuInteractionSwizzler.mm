/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "EditMenuInteractionSwizzler.h"

#if HAVE(UI_EDIT_MENU_INTERACTION)

#import "TestController.h"
#import <UIKit/UIKit.h>
#import <wtf/WeakObjCPtr.h>

@interface EditMenuInteractionDelegateWrapper : NSObject<UIEditMenuInteractionDelegate>
- (instancetype)initWithDelegate:(id<UIEditMenuInteractionDelegate>)delegate;
@end

@implementation EditMenuInteractionDelegateWrapper {
    __weak id<UIEditMenuInteractionDelegate> _delegate;
}

- (instancetype)initWithDelegate:(id<UIEditMenuInteractionDelegate>)delegate
{
    if (!(self = [super init]))
        return nil;

    _delegate = delegate;
    return self;
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
    if (![_delegate respondsToSelector:invocation.selector])
        return;

    @try {
        [invocation invokeWithTarget:_delegate];
    } @catch (id exception) {
        NSLog(@"Caught exception: %@ while forwarding invocation: %@", exception, invocation);
    }
}

- (BOOL)respondsToSelector:(SEL)selector
{
    return [super respondsToSelector:selector] || [_delegate respondsToSelector:selector];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector
{
    return [(NSObject *)_delegate methodSignatureForSelector:selector];
}

#pragma mark - UIEditMenuInteractionDelegate

- (void)editMenuInteraction:(UIEditMenuInteraction *)interaction willPresentMenuForConfiguration:(UIEditMenuConfiguration *)configuration animator:(id<UIEditMenuInteractionAnimating>)animator
{
    if ([_delegate respondsToSelector:@selector(editMenuInteraction:willPresentMenuForConfiguration:animator:)])
        [_delegate editMenuInteraction:interaction willPresentMenuForConfiguration:configuration animator:animator];

    [animator addCompletion:[weakInteraction = WeakObjCPtr<UIEditMenuInteraction>(interaction)] {
        if (auto strongInteraction = weakInteraction.get())
            WTR::TestController::singleton().didPresentEditMenuInteraction(strongInteraction.get());
    }];
}

- (void)editMenuInteraction:(UIEditMenuInteraction *)interaction willDismissMenuForConfiguration:(UIEditMenuConfiguration *)configuration animator:(id<UIEditMenuInteractionAnimating>)animator
{
    if ([_delegate respondsToSelector:@selector(editMenuInteraction:willDismissMenuForConfiguration:animator:)])
        [_delegate editMenuInteraction:interaction willDismissMenuForConfiguration:configuration animator:animator];

    [animator addCompletion:[weakInteraction = WeakObjCPtr<UIEditMenuInteraction>(interaction)] {
        if (auto strongInteraction = weakInteraction.get())
            WTR::TestController::singleton().didDismissEditMenuInteraction(strongInteraction.get());
    }];
}

@end

@interface UIEditMenuInteraction (WebKitTestRunner)
- (instancetype)swizzled_initWithDelegate:(id<UIEditMenuInteractionDelegate>)delegate;
@end

@implementation UIEditMenuInteraction (WebKitTestRunner)

- (instancetype)swizzled_initWithDelegate:(id<UIEditMenuInteractionDelegate>)delegate
{
    static void* delegateWrapperKey;

    auto delegateWrapper = adoptNS([[EditMenuInteractionDelegateWrapper alloc] initWithDelegate:delegate]);
    objc_setAssociatedObject(delegate, &delegateWrapperKey, delegateWrapper.get(), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return [self swizzled_initWithDelegate:delegateWrapper.get()];
}

@end

namespace WTR {

EditMenuInteractionSwizzler::EditMenuInteractionSwizzler()
    : m_originalMethod(class_getInstanceMethod(UIEditMenuInteraction.class, @selector(initWithDelegate:)))
    , m_swizzledMethod(class_getInstanceMethod(UIEditMenuInteraction.class, @selector(swizzled_initWithDelegate:)))
{
    m_originalImplementation = method_getImplementation(m_originalMethod);
    m_swizzledImplementation = method_getImplementation(m_swizzledMethod);
    class_replaceMethod(UIEditMenuInteraction.class, @selector(swizzled_initWithDelegate:), m_originalImplementation, method_getTypeEncoding(m_originalMethod));
    class_replaceMethod(UIEditMenuInteraction.class, @selector(initWithDelegate:), m_swizzledImplementation, method_getTypeEncoding(m_swizzledMethod));
}

EditMenuInteractionSwizzler::~EditMenuInteractionSwizzler()
{
    class_replaceMethod(UIEditMenuInteraction.class, @selector(swizzled_initWithDelegate:), m_swizzledImplementation, method_getTypeEncoding(m_originalMethod));
    class_replaceMethod(UIEditMenuInteraction.class, @selector(initWithDelegate:), m_originalImplementation, method_getTypeEncoding(m_swizzledMethod));
}

} // namespace WTR

#endif // HAVE(UI_EDIT_MENU_INTERACTION)
