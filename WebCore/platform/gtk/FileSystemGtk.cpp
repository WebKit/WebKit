/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * All rights reserved.
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

#include "FileSystem.h"
#include "PlatformString.h"
#include "CString.h"

#include <glib.h>
#include <glib/gstdio.h>

namespace WebCore {

bool fileExists(const String& path)
{
    bool result = false;
    gchar* filename = g_filename_from_utf8(path.utf8().data(), -1, 0, 0, 0);

    if (filename) {
        result = g_file_test(filename, G_FILE_TEST_EXISTS);
        g_free(filename);
    }
    
    return result;
}

bool deleteFile(const String& path)
{
    bool result = false;
    gchar* filename = g_filename_from_utf8(path.utf8().data(), -1, 0, 0, 0);

    if (filename) {
        result = g_remove(filename) == 0;
        g_free(filename);
    }
    
    return result;
}

bool fileSize(const String& path, long long& resultSize)
{
    gchar* filename = g_filename_from_utf8(path.utf8().data(), -1, 0, 0, 0);
    if (!filename)
        return false;

    struct stat statResult;
    gint result = g_stat(filename, &statResult);
    g_free(filename);
    if (result != 0)
        return false;

    resultSize = statResult.st_size;
    return true;
}

}
