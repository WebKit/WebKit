/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
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

#if PLATFORM(KDE)
#include <kio/job.h>
#endif

#include <QRegExp>

#include "FrameQt.h"
#include "DocLoader.h"
#include "ResourceHandle.h"
#include "DeprecatedString.h"
#include "ResourceHandleManager.h"
#include "ResourceHandleInternal.h"

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
    cancel();
}

bool ResourceHandle::start(DocLoader* docLoader)
{
    ref();

    FrameQt* frame = 0;
    if (docLoader) {
        frame = QtFrame(docLoader->frame());
        ASSERT(frame);
    }

    ResourceHandleManager::self()->add(this, frame ? frame->client() : 0);
    return true;
}

void ResourceHandle::cancel()
{
    ResourceHandleManager::self()->cancel(this);
}

QString ResourceHandle::extractCharsetFromHeaders(QString headers) const
{
    int pos = headers.find("content-type:", 0, false);

    if (pos > -1) {
        pos += 13;

        int index = headers.find('\n', pos);
        QString type = headers.mid(pos, index - pos);
        index = type.indexOf(';');

        if (index > -1)
            return type.mid(index + 1).remove(QRegExp("charset[ ]*=[ ]*", Qt::CaseInsensitive)).trimmed();
    }

    return QString();
}

} // namespace WebCore
