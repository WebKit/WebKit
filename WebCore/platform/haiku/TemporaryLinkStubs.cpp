/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
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

#include "AXObjectCache.h"
#include "CachedResource.h"
#include "CookieJar.h"
#include "Cursor.h"
#include "DataGridColumnList.h"
#include "FileSystem.h"
#include "Font.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "History.h"
#include "IconLoader.h"
#include "InspectorController.h"
#include "IntPoint.h"
#include "KURL.h"
#include "Language.h"
#include "Node.h"
#include "NotImplemented.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "PlatformScrollBar.h"
#include "PluginInfoStore.h"
#include "RenderTheme.h"
#include "Screen.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "SharedBuffer.h"
#include "TextBoundaries.h"
#include "Threading.h"
#include "Widget.h"
#include "loader.h"
#include <runtime/JSValue.h>
#include <stdio.h>
#include <stdlib.h>

using namespace WebCore;

Vector<char> loadResourceIntoArray(const char*)
{
    notImplemented();
    return Vector<char>();
}

namespace WebCore {

bool historyContains(String const&)
{
    return false;
}

Vector<String> supportedKeySizes()
{
    notImplemented();
    return Vector<String>();
}

String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String &challengeString, const KURL &url)
{
    return String();
}

float userIdleTime()
{
    notImplemented();
    return 0;
}

void callOnMainThread(void (*)())
{
    notImplemented();
}

PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String&)
{
    notImplemented();
    return 0;
}

String KURL::fileSystemPath() const
{
    notImplemented();
    return String();
}

void getSupportedKeySizes(Vector<String>&)
{
    notImplemented();
}

} // namespace WebCore

