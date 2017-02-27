/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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

class InlineLayoutSupport
{

    constructor(controls, droppableControls)
    {
        this._controls = controls;
        this._droppableControls = droppableControls;
    }

    // Public

    childrenAfterPerformingLayout()
    {
        const controls = this._controls;

        // Reset dropped buttons.
        controls.rightContainer.buttons.concat(controls.leftContainer.buttons).forEach(button => delete button.dropped);

        controls.leftContainer.layout();
        controls.rightContainer.layout();

        let shouldShowTimeControl = !controls.statusLabel.enabled;

        if (shouldShowTimeControl)
            controls.timeControl.width = controls.width - controls.leftContainer.width - controls.rightContainer.width;

        if (shouldShowTimeControl && controls.timeControl.isSufficientlyWide)
            controls.timeControl.x = controls.leftContainer.width;
        else {
            let droppedControls = false;
            shouldShowTimeControl = false;

            // Since we don't have enough space to display the scrubber, we may also not have
            // enough space to display all buttons in the left and right containers, so gradually drop them.
            for (let button of this._droppableControls) {
                // Nothing left to do if the combined container widths is shorter than the available width.
                if (controls.leftContainer.width + controls.rightContainer.width < controls.width)
                    break;

                droppedControls = true;

                // If the button was already not participating in layout, we can skip it.
                if (!button.visible)
                    continue;

                // This button must now be dropped.
                button.dropped = true;

                controls.leftContainer.layout();
                controls.rightContainer.layout();
            }

            // We didn't need to drop controls and we have status text to show.
            if (!droppedControls && controls.statusLabel.enabled) {
                controls.statusLabel.x = controls.leftContainer.width;
                controls.statusLabel.width = controls.width - controls.leftContainer.width - controls.rightContainer.width;
            }
        }

        controls.rightContainer.x = controls.width - controls.rightContainer.width;

        const children = [controls.leftContainer];
        if (shouldShowTimeControl)
            children.push(controls.timeControl);
        else if (controls.statusLabel.enabled)
            children.push(controls.statusLabel);
        children.push(controls.rightContainer);
        return children;
    }

}
