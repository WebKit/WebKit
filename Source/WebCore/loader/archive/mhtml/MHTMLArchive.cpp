/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MHTML)
#include "MHTMLArchive.h"

#include "MHTMLParser.h"
#include "MIMEHeader.h"

namespace WebCore {

MHTMLArchive::MHTMLArchive()
{
}

PassRefPtr<MHTMLArchive> MHTMLArchive::create()
{
    return adoptRef(new MHTMLArchive);
}

PassRefPtr<MHTMLArchive> MHTMLArchive::create(const KURL& url, SharedBuffer* data)
{
    // For security reasons we only load MHTML pages from the local file system.
    if (!url.isLocalFile())
        return false;

    MHTMLParser parser(data);
    RefPtr<MHTMLArchive> mainArchive = parser.parseArchive();
    if (!mainArchive)
        return 0; // Invalid MHTML file.

    // Since MHTML is a flat format, we need to make all frames aware of all resources.
    for (size_t i = 0; i < parser.frameCount(); ++i) {
        RefPtr<MHTMLArchive> archive = parser.frameAt(i);
        for (size_t j = 0; j < parser.frameCount(); ++j) {
            if (i != j)
                archive->addSubframeArchive(parser.frameAt(j));
        }
        for (size_t j = 0; j < parser.subResourceCount(); ++j)
            archive->addSubresource(parser.subResourceAt(j));
    }
    return mainArchive.release();
}

}
#endif
