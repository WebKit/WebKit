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

class FormattingContext {
    constructor(rootContainer) {
        this.m_rootContainer = rootContainer;
        this.m_floatingContext = null;
    }

    rootContainer() {
        return this.m_rootContainer;
    }

    floatingContext() {
        return this.m_floatingContext;
    }

    layout(layoutContext) {
    }

    computeWidth(layoutBox) {
    }

    computeHeight(layoutBox) {
    }

    marginTop(layoutBox) {
        return Utils.computedMarginTop(layoutBox.node());
    }
    
    marginLeft(layoutBox) {
        return Utils.computedMarginLeft(layoutBox.node());
    }
    
    marginBottom(layoutBox) {
        return Utils.computedMarginBottom(layoutBox.node());
    }
    
    marginRight(layoutBox) {
        return Utils.computedMarginRight(layoutBox.node());
    }

    absoluteMarginBox(layoutBox) {
        let absoluteContentBox = Utils.mapToContainer(layoutBox, this.rootContainer());
        absoluteContentBox.moveBy(new LayoutSize(-this.marginLeft(layoutBox), -this.marginTop(layoutBox)));
        absoluteContentBox.growBy(new LayoutSize(this.marginLeft(layoutBox) + this.marginRight(layoutBox), this.marginTop(layoutBox) + this.marginBottom(layoutBox)));
        return absoluteContentBox;
    }

    absoluteBorderBox(layoutBox) {
        let borderBox = layoutBox.borderBox();
        let absoluteRect = new LayoutRect(Utils.mapToContainer(layoutBox, this.rootContainer()).topLeft(), borderBox.size());
        absoluteRect.moveBy(borderBox.topLeft());
        return absoluteRect;
    }

    absolutePaddingBox(layoutBox) {
        let paddingBox = layoutBox.paddingBox();
        let absoluteRect = new LayoutRect(Utils.mapToContainer(layoutBox, this.rootContainer()).topLeft(), paddingBox.size());
        absoluteRect.moveBy(paddingBox.topLeft());
        return absoluteRect;
    }

    absoluteContentBox(layoutBox) {
        let contentBox = layoutBox.contentBox();
        let absoluteRect = new LayoutRect(Utils.mapToContainer(layoutBox, this.rootContainer()).topLeft(), contentBox.size());
        absoluteRect.moveBy(contentBox.topLeft());
        return absoluteRect;
    }
}
