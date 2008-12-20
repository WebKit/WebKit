/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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
#include "ContextMenuClientWx.h"
#include "NotImplemented.h"
#include "ContextMenu.h"
#include <stdio.h>

namespace WebCore {

ContextMenuClientWx::~ContextMenuClientWx()
{
}

void ContextMenuClientWx::contextMenuDestroyed()
{
    delete this;
}

PlatformMenuDescription ContextMenuClientWx::getCustomMenuFromDefaultItems(ContextMenu* menu)
{
    return menu->releasePlatformDescription();
}

void ContextMenuClientWx::contextMenuItemSelected(ContextMenuItem*,
                                                   const ContextMenu*)
{
    notImplemented();
}

void ContextMenuClientWx::downloadURL(const KURL&)
{
    notImplemented();
}

void ContextMenuClientWx::copyImageToClipboard(const HitTestResult&)
{
    notImplemented();
}

void ContextMenuClientWx::searchWithGoogle(const Frame*)
{
    notImplemented();
}

void ContextMenuClientWx::lookUpInDictionary(Frame*)
{
    notImplemented();
}

void ContextMenuClientWx::speak(const String&)
{
    notImplemented();
}

void ContextMenuClientWx::stopSpeaking()
{
    notImplemented();
}

}
