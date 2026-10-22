/*
 * Copyright (C) 2013-2026 Apple Inc. All rights reserved.
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

#import "TestWebKitAPISupport.h"

#import "TestsController.h"
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#endif

void TestWebKitAPIEnableAllSDKAlignedBehaviors(void)
{
    WTF::enableAllSDKAlignedBehaviors();
}

#if PLATFORM(MAC)

using PasteboardWithNameFunction = NSPasteboard *(*)(id, SEL, NSPasteboardName);
static PasteboardWithNameFunction originalPasteboardWithName;

static NSPasteboard *isolatedPasteboard(NSPasteboardName name)
{
    static NeverDestroyed<RetainPtr<NSDictionary<NSPasteboardName, NSPasteboard *>>> pasteboards = [] -> RetainPtr<NSDictionary<NSPasteboardName, NSPasteboard *>> {
        RetainPtr pasteboards = adoptNS([[NSMutableDictionary alloc] init]);
        for (NSPasteboardName standardName in @[ NSPasteboardNameGeneral, NSPasteboardNameDrag, NSPasteboardNameFind, NSPasteboardNameFont, NSPasteboardNameRuler ]) {
            // A run-api-tests worker runs its test processes one at a time, so they can share pasteboards.
            RetainPtr pasteboard = originalPasteboardWithName(NSPasteboard.class, @selector(pasteboardWithName:), [NSString stringWithFormat:@"%@ (TestWebKitAPI %d)", standardName, getppid()]);
            [pasteboard clearContents];
            [pasteboards setObject:pasteboard.get() forKey:standardName];
        }
        atexit_b(^{
            for (NSPasteboard *pasteboard in [pasteboards objectEnumerator])
                [pasteboard releaseGlobally];
        });
        return pasteboards;
    }();
    return [pasteboards.get() objectForKey:name];
}

static NSPasteboard *isolatedPasteboardWithName(id self, SEL selector, NSPasteboardName name)
{
    if (NSPasteboard *pasteboard = isolatedPasteboard(name))
        return pasteboard;
    return originalPasteboardWithName(self, selector, name);
}

static NSPasteboard *isolatedGeneralPasteboard(id, SEL)
{
    return isolatedPasteboard(NSPasteboardNameGeneral);
}

void TestWebKitAPIIsolatePasteboards(void)
{
    Method pasteboardWithName = class_getClassMethod(NSPasteboard.class, @selector(pasteboardWithName:));
    originalPasteboardWithName = reinterpret_cast<PasteboardWithNameFunction>(method_getImplementation(pasteboardWithName));
    method_setImplementation(pasteboardWithName, reinterpret_cast<IMP>(isolatedPasteboardWithName));
    method_setImplementation(class_getClassMethod(NSPasteboard.class, @selector(generalPasteboard)), reinterpret_cast<IMP>(isolatedGeneralPasteboard));
}

#endif // PLATFORM(MAC)

BOOL TestWebKitAPIRunTests(int argc, char** argv, bool *didRunAnyTest)
{
    return TestWebKitAPI::TestsController::singleton().run(argc, argv, didRunAnyTest);
}
