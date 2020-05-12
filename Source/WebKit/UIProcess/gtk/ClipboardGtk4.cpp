/*
 * Copyright (C) 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Clipboard.h"

#if USE(GTK4)

#include <WebCore/SelectionData.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {

Clipboard::Clipboard(Type)
{
}

Clipboard::Type Clipboard::type() const
{
    return Type::Clipboard;
}

void Clipboard::formats(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler({ });
}

void Clipboard::readText(CompletionHandler<void(String&&)>&& completionHandler)
{
    completionHandler({ });
}

void Clipboard::readFilePaths(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler({ });
}

void Clipboard::readBuffer(const char* format, CompletionHandler<void(Ref<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    completionHandler(WebCore::SharedBuffer::create());
}

void Clipboard::write(const WebCore::SelectionData&)
{
}

void Clipboard::clear()
{
}

} // namespace WebKit

#endif
