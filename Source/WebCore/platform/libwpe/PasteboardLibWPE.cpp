/*
 * Copyright (C) 2014-2015 Igalia S.L.
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

#include "config.h"
#include "Pasteboard.h"

#if USE(LIBWPE)

#include "NotImplemented.h"
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"

namespace WebCore {

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste()
{
    return std::make_unique<Pasteboard>();
}

Pasteboard::Pasteboard()
{
}

bool Pasteboard::hasData()
{
    // FIXME: Getting the list of types for this is wasteful. Do this in the UI process.
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types);
    return !types.isEmpty();
}

Vector<String> Pasteboard::typesSafeForBindings(const String&)
{
    notImplemented();
    return { };
}

Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types);
    return types;
}

String Pasteboard::readOrigin()
{
    notImplemented(); // webkit.org/b/177633: [GTK] Move to new Pasteboard API
    return { };
}

String Pasteboard::readString(const String& type)
{
    return platformStrategies()->pasteboardStrategy()->readStringFromPasteboard(0, type);
}

String Pasteboard::readStringInCustomData(const String&)
{
    notImplemented();
    return { };
}

void Pasteboard::writeString(const String& type, const String& text)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(type, text);
}

void Pasteboard::clear()
{
}

void Pasteboard::clear(const String&)
{
}

void Pasteboard::read(PasteboardPlainText& text)
{
    text.text = platformStrategies()->pasteboardStrategy()->readStringFromPasteboard(0, "text/plain;charset=utf-8");
}

void Pasteboard::read(PasteboardWebContentReader&, WebContentReadingPolicy)
{
    notImplemented();
}

void Pasteboard::read(PasteboardFileReader&)
{
}

void Pasteboard::write(const PasteboardURL& url)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard("text/plain;charset=utf-8", url.url.string());
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL&)
{
    notImplemented();
}

void Pasteboard::write(const PasteboardImage&)
{
}

void Pasteboard::write(const PasteboardWebContent& content)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(content);
}

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    notImplemented();
    return FileContentState::NoFileOrImageData;
}

bool Pasteboard::canSmartReplace()
{
    return false;
}

void Pasteboard::writeMarkup(const String&)
{
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption)
{
    writeString("text/plain;charset=utf-8", text);
}

void Pasteboard::writeCustomData(const PasteboardCustomData&)
{
}

void Pasteboard::write(const Color&)
{
}

} // namespace WebCore

#endif // USE(LIBWPE)
