/*
 * Copyright (C) 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Google Inc.
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
#include "ClipboardChromium.h"

#include "ChromiumDataObject.h"

namespace WebCore {
    

// Filename length and character-set limits vary by filesystem, and there's no
// real way to tell what filesystem is in use. Since HFS+ is by far the most
// common, we'll abide by its limits, which are 255 Unicode characters (slightly
// simplified; really it uses Normalization Form D, but the differences aren't
// really worth dealing with here.)
static const unsigned MaxHFSFilenameLength = 255;


static bool isInvalidFileCharacter(UChar c)
{
    // HFS+ basically allows anything but '/'. For sanity's sake we'll disallow
    // control characters.
    return c < ' ' || c == 0x7F || c == '/';
}

String ClipboardChromium::validateFileName(const String& title, ChromiumDataObject* dataObject)
{
    // Remove any invalid file system characters, especially "/".
    String result = title.removeCharacters(isInvalidFileCharacter);
    String extension = dataObject->fileExtension().removeCharacters(&isInvalidFileCharacter);

    // Remove a ridiculously-long extension.
    if (extension.length() >= MaxHFSFilenameLength)
        extension = "";
    
    // Truncate an overly-long filename.
    int overflow = result.length() + extension.length() - MaxHFSFilenameLength;
    if (overflow > 0) 
        result.remove(result.length() - overflow, overflow);
    
    dataObject->setFileExtension(extension);
    return result;
}

}  // namespace WebCore
