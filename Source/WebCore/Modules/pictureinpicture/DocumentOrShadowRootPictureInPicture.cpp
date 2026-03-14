/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(PICTURE_IN_PICTURE_API)
#include "DocumentOrShadowRootPictureInPicture.h"

#include "Document.h"
#include "HTMLVideoElement.h"
#include "TreeScopeInlines.h"

namespace WebCore {

// https://w3c.github.io/picture-in-picture/#documentorshadowroot-extension
Element* DocumentOrShadowRootPictureInPicture::pictureInPictureElement(TreeScope& treeScope)
{
    // The pictureInPictureElement attribute’s getter must run these steps:
    // 1. If this is a shadow root and its host is not connected, return null and abort these steps.
    if (auto* shadowHost = treeScope.rootNode().shadowHost(); shadowHost && shadowHost->isConnected())
        return nullptr;

    // 2. Let candidate be the result of retargeting Picture-in-Picture element against this.
    // 3. If candidate and this are in the same tree, return candidate and abort these steps.
    Ref document = treeScope.documentScope();
    if (RefPtr pictureInPictureElement = document->pictureInPictureElement())
        return treeScope.ancestorElementInThisScope(pictureInPictureElement.get());

    // 4. Return null.
    return nullptr;
}

} // namespace WebCore

#endif
