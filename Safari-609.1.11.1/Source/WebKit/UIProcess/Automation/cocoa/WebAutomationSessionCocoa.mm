/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#import "WebAutomationSession.h"

#if PLATFORM(COCOA)

#import <wtf/FileSystem.h>

#if PLATFORM(IOS_FAMILY)
#include <ImageIO/CGImageDestination.h>
#include <MobileCoreServices/UTCoreTypes.h>
#include <WebCore/KeyEventCodesIOS.h>
#endif

namespace WebKit {
using namespace WebCore;

Optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(const ShareableBitmap::Handle& imageDataHandle)
{
    auto bitmap = ShareableBitmap::create(imageDataHandle, SharedMemory::Protection::ReadOnly);
    if (!bitmap)
        return WTF::nullopt;

    RetainPtr<CGImageRef> cgImage = bitmap->makeCGImage();
    RetainPtr<NSMutableData> imageData = adoptNS([[NSMutableData alloc] init]);
    RetainPtr<CGImageDestinationRef> destination = adoptCF(CGImageDestinationCreateWithData((CFMutableDataRef)imageData.get(), kUTTypePNG, 1, 0));
    if (!destination)
        return WTF::nullopt;

    CGImageDestinationAddImage(destination.get(), cgImage.get(), 0);
    CGImageDestinationFinalize(destination.get());

    return String([imageData base64EncodedStringWithOptions:0]);
}

Optional<String> WebAutomationSession::platformGenerateLocalFilePathForRemoteFile(const String& remoteFilePath, const String& base64EncodedFileContents)
{
    RetainPtr<NSData> fileContents = adoptNS([[NSData alloc] initWithBase64EncodedString:base64EncodedFileContents options:0]);
    if (!fileContents) {
        LOG_ERROR("WebAutomationSession: unable to decode base64-encoded file contents.");
        return WTF::nullopt;
    }

    NSString *temporaryDirectory = FileSystem::createTemporaryDirectory(@"WebDriver");
    NSURL *remoteFile = [NSURL fileURLWithPath:remoteFilePath isDirectory:NO];
    NSString *localFilePath = [temporaryDirectory stringByAppendingPathComponent:remoteFile.lastPathComponent];

    NSError *fileWriteError;
    [fileContents.get() writeToFile:localFilePath options:NSDataWritingAtomic error:&fileWriteError];
    if (fileWriteError) {
        LOG_ERROR("WebAutomationSession: Error writing image data to temporary file: %@", fileWriteError);
        return WTF::nullopt;
    }

    return String(localFilePath);
}

Optional<unichar> WebAutomationSession::charCodeForVirtualKey(Inspector::Protocol::Automation::VirtualKey key) const
{
    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::Shift:
    case Inspector::Protocol::Automation::VirtualKey::Control:
    case Inspector::Protocol::Automation::VirtualKey::Alternate:
    case Inspector::Protocol::Automation::VirtualKey::Meta:
    case Inspector::Protocol::Automation::VirtualKey::Command:
        return WTF::nullopt;
    case Inspector::Protocol::Automation::VirtualKey::Help:
        return NSHelpFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Backspace:
        return NSBackspaceCharacter;
    case Inspector::Protocol::Automation::VirtualKey::Tab:
        return NSTabCharacter;
    case Inspector::Protocol::Automation::VirtualKey::Clear:
        return NSClearLineFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Enter:
        return NSEnterCharacter;
    case Inspector::Protocol::Automation::VirtualKey::Pause:
        return NSPauseFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Cancel:
        // The 'cancel' key does not exist on Apple keyboards and has no keycode.
        // According to the internet its functionality is similar to 'Escape'.
    case Inspector::Protocol::Automation::VirtualKey::Escape:
        return 0x1B;
    case Inspector::Protocol::Automation::VirtualKey::PageUp:
        return NSPageUpFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::PageDown:
        return NSPageDownFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::End:
        return NSEndFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Home:
        return NSHomeFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::LeftArrow:
        return NSLeftArrowFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::UpArrow:
        return NSUpArrowFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::RightArrow:
        return NSRightArrowFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::DownArrow:
        return NSDownArrowFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Insert:
        return NSInsertFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Delete:
        return NSDeleteFunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Space:
        return ' ';
    case Inspector::Protocol::Automation::VirtualKey::Semicolon:
        return ';';
    case Inspector::Protocol::Automation::VirtualKey::Equals:
        return '=';
    case Inspector::Protocol::Automation::VirtualKey::Return:
        return NSCarriageReturnCharacter;
    case Inspector::Protocol::Automation::VirtualKey::NumberPad0:
        return '0';
    case Inspector::Protocol::Automation::VirtualKey::NumberPad1:
        return '1';
    case Inspector::Protocol::Automation::VirtualKey::NumberPad2:
        return '2';
    case Inspector::Protocol::Automation::VirtualKey::NumberPad3:
        return '3';
    case Inspector::Protocol::Automation::VirtualKey::NumberPad4:
        return '4';
    case Inspector::Protocol::Automation::VirtualKey::NumberPad5:
        return '5';
    case Inspector::Protocol::Automation::VirtualKey::NumberPad6:
        return '6';
    case Inspector::Protocol::Automation::VirtualKey::NumberPad7:
        return '7';
    case Inspector::Protocol::Automation::VirtualKey::NumberPad8:
        return '8';
    case Inspector::Protocol::Automation::VirtualKey::NumberPad9:
        return '9';
    case Inspector::Protocol::Automation::VirtualKey::NumberPadMultiply:
        return '*';
    case Inspector::Protocol::Automation::VirtualKey::NumberPadAdd:
        return '+';
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSubtract:
        return '-';
    case Inspector::Protocol::Automation::VirtualKey::NumberPadSeparator:
        // The 'Separator' key is only present on a few international keyboards.
        // It is usually mapped to the same character as Decimal ('.' or ',').
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDecimal:
        return '.';
    case Inspector::Protocol::Automation::VirtualKey::NumberPadDivide:
        return '/';
    case Inspector::Protocol::Automation::VirtualKey::Function1:
        return NSF1FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function2:
        return NSF2FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function3:
        return NSF3FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function4:
        return NSF4FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function5:
        return NSF5FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function6:
        return NSF6FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function7:
        return NSF7FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function8:
        return NSF8FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function9:
        return NSF9FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function10:
        return NSF10FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function11:
        return NSF11FunctionKey;
    case Inspector::Protocol::Automation::VirtualKey::Function12:
        return NSF12FunctionKey;
    default:
        return WTF::nullopt;
    }
}

Optional<unichar> WebAutomationSession::charCodeIgnoringModifiersForVirtualKey(Inspector::Protocol::Automation::VirtualKey key) const
{
    switch (key) {
    case Inspector::Protocol::Automation::VirtualKey::NumberPadMultiply:
        return '8';
    case Inspector::Protocol::Automation::VirtualKey::NumberPadAdd:
        return '=';
    default:
        return charCodeForVirtualKey(key);
    }
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
