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
class FloatingState {
public:
    addFloating(Display::Box&, bool isFloatingLeft) {
    Vector<Display::Box&> leftFloatingStack();
    Vector<Display::Box&> rightFloatingStack();
    Display::Box& lastFloating();

    FormattingState& formattingState();
};
*/
class FloatingState {
    constructor(parentFormattingState) {
        this.m_parentFormattingState = parentFormattingState;
        this.m_leftFloatingBoxStack = new Array();
        this.m_rightFloatingBoxStack = new Array();
        this.m_lastFloating = null;
    }

    addFloating(displayBox, isFloatingLeft) {
        this.m_lastFloating = displayBox;
        if (isFloatingLeft) {
            this.m_leftFloatingBoxStack.push(displayBox);
            return;
        }
        this.m_rightFloatingBoxStack.push(displayBox);
    }

    leftFloatingStack() {
        return this.m_leftFloatingBoxStack;
    }

    rightFloatingStack() {
        return this.m_rightFloatingBoxStack;
    }

    lastFloating() {
        return this.m_lastFloating;
    }

    formattingState() {
        return this.m_parentFormattingState;
    }
}
