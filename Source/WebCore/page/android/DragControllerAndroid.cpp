/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "DragController.h"

#include "DragData.h"
#include "NotImplemented.h"

namespace WebCore {

bool DragController::isCopyKeyDown()
{
    return false;
}
    
DragOperation DragController::dragOperation(DragData* dragData)
{
    // FIXME: This logic is incomplete
    notImplemented();
    if (dragData->containsURL())
        return DragOperationCopy;

    return DragOperationNone;
}

void DragController::cleanupAfterSystemDrag()
{
}

const float DragController::DragImageAlpha = 1.0f;
static IntSize dummy;
const IntSize& DragController::maxDragImageSize() { return dummy; }
const int DragController::DragIconRightInset = 0;
const int DragController::DragIconBottomInset = 0;
const int DragController::LinkDragBorderInset = 0;
const int DragController::MaxOriginalImageArea = 0;

}  // namespace WebCore
