/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "PlugInOriginHash.h"

#include "Document.h"
#include "Frame.h"
#include "HTMLPlugInImageElement.h"
#include "KURL.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include <wtf/text/StringHash.h>

namespace WebCore {

static inline void addCaseFoldedCharacters(StringHasher& hasher, const String& string)
{
    if (string.isEmpty())
        return;
    if (string.is8Bit())
        return hasher.addCharacters<LChar, CaseFoldingHash::foldCase<LChar> >(string.characters8(), string.length());
    return hasher.addCharacters<UChar, CaseFoldingHash::foldCase<UChar> >(string.characters16(), string.length());
}

unsigned PlugInOriginHash::hash(HTMLPlugInImageElement* plugInElement, const KURL& plugInURL)
{
    ASSERT(plugInElement->document()->page());

    String mimeType = plugInElement->serviceType();
    if (mimeType.isEmpty())
        mimeType = mimeTypeFromURL(plugInURL);

    // We want to avoid concatenating the strings and then taking the hash, since that could lead to an expensive conversion.
    // We also want to avoid using the hash() function in StringImpl or CaseFoldingHash because that masks out bits for the use of flags.
    StringHasher hasher;
    addCaseFoldedCharacters(hasher, plugInElement->document()->page()->mainFrame()->document()->baseURL().host());
    hasher.addCharacter(0);
    addCaseFoldedCharacters(hasher, plugInURL.host());
    hasher.addCharacter(0);
    addCaseFoldedCharacters(hasher, mimeType);
    LOG(Plugins, "Hash: %s %s %s", plugInElement->document()->page()->mainFrame()->document()->baseURL().host().utf8().data(), plugInURL.host().utf8().data(), mimeType.utf8().data());
    return hasher.hash();
}

} // namespace WebCore
