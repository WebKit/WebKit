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

#include <JavaScriptCore/JavaScript.h>

#if JSC_OBJC_API_ENABLED

#import <JavaScriptCore/JSVirtualMachine.h>

@interface JSVirtualMachine(JSPrivate)

- (void)shrinkFootprint; // FIXME: Remove this SPI when clients move to shrinkFootprintWhenIdle: https://bugs.webkit.org/show_bug.cgi?id=186071

/*!
@method
@discussion Shrinks the memory footprint of the VM by deleting various internal caches,
 running synchronous garbage collection, and releasing memory back to the OS. Note: this
 API waits until no JavaScript is running on the stack before it frees any memory. It's
 best to call this API when no JavaScript is running on the stack for this reason. However, if
 you do call this API when JavaScript is running on the stack, the API will wait until all JavaScript
 on the stack finishes running to free memory back to the OS. Therefore, calling this
 API may not synchronously free memory.
*/

- (void)shrinkFootprintWhenIdle; // FIXME: Annotate this with NS_AVAILABLE: <rdar://problem/40071332>.

@end

#endif // JSC_OBJC_API_ENABLED
