/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

class Line {
    constructor(topLeft, height, availableWidth) {
        this.m_availableWidth = availableWidth;
        this.m_lineRect = new LayoutRect(topLeft, new LayoutSize(0, height));
        this.m_lineBoxes = new Array();
    }

    isEmpty() {
        return !this.m_lineBoxes.length;
    }

    availableWidth() {
        return this.m_availableWidth;
    }

    rect() {
        return this.m_lineRect;
    }

    lineBoxes() {
        return this.m_lineBoxes;
    }

    addLineBox(startPosition, endPosition, size) {
        this.m_availableWidth -= size.width();
        // TODO: use the actual height instead of the line height.
        let lineBoxRect = new LayoutRect(this.rect().topLeft(), new LayoutSize(size.width(), this.rect().height()));
        this.m_lineBoxes.push({startPosition, endPosition, lineBoxRect});
        this.m_lineRect.growBy(new LayoutSize(size.width(), 0));
    }
}
