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

/*
class InlineFormattingState : public FormattingState {
public:
    Vector<Line> lines();
    void appendLine(Line);
};
*/
class InlineFormattingState extends FormattingState {
    constructor(formattingRoot, layoutState) {
        super(layoutState, formattingRoot);
        // If the block container box that initiates this inline formatting context also establishes a block context, create a new float for us.
        if (this.formattingRoot().establishesBlockFormattingContext())
            this.m_floatingState = new FloatingState(this);
        else {
            // Find the formatting state in which this formatting root lives, not the one it creates (this).
            let parentFormattingState = layoutState.formattingStateForBox(formattingRoot);
            this.m_floatingState = parentFormattingState.floatingState();
        }
        this.m_lines = new Array();
    }

    lines() {
        return this.m_lines;
    }

    appendLine(line) {
        this.m_lines.push(line);
    }
}
