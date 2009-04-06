/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
#include "FileChooser.h"

#include "CString.h"
#include "FileSystem.h"
#include "Icon.h"
#include "LocalizedStrings.h"
#include "StringTruncator.h"

#include <glib.h>
#include <gtk/gtk.h>

namespace WebCore {

static bool stringByAdoptingFileSystemRepresentation(gchar* systemFilename, String& result)
{
    if (!systemFilename)
        return false;

    result = filenameToString(systemFilename);
    g_free(systemFilename);

    return true;
}

String FileChooser::basenameForWidth(const Font& font, int width) const
{
    if (width <= 0)
        return String();

    String string = fileButtonNoFileSelectedLabel();

    if (m_filenames.size() == 1) {
        gchar* systemFilename = filenameFromString(m_filenames[0]);
        gchar* systemBasename = g_path_get_basename(systemFilename);
        g_free(systemFilename);
        stringByAdoptingFileSystemRepresentation(systemBasename, string);
    } else if (m_filenames.size() > 1)
        return StringTruncator::rightTruncate(multipleFileUploadText(m_filenames.size()), width, font, false);

    return StringTruncator::centerTruncate(string, width, font, false);
}
}
