/*
 * Copyright 2006, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#include "FileChooser.h"

#include "Font.h"

namespace WebCore {

String FileChooser::basenameForWidth(const Font& font, int width) const 
{
    if (!m_filenames.size())
        return String();
    // FIXME: This could be a lot faster, but assuming the data will not
    // often be much longer than the provided width, this may be fast enough.
    String output = m_filenames[0].copy();
    while (font.width(TextRun(output.impl())) > width && output.length() > 4) {
        output = output.replace(output.length() - 4, 4, String("..."));
    }
    return output;
}

// The following two strings are used for File Upload form control, ie
// <input type="file">. The first is the text that appears on the button
// that when pressed, the user can browse for and select a file. The
// second string is rendered on the screen when no file has been selected.
String fileButtonChooseFileLabel()
{
    return String("Uploads Disabled");
}

String fileButtonNoFileSelectedLabel()
{
    return String("No file selected");
}

} // namesapce WebCore

