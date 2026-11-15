/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

WI.ScreenshotsTimelineView = class ScreenshotsTimelineView extends WI.TimelineView
{
    constructor(timeline, extraArguments)
    {
        console.assert(timeline.type === WI.TimelineRecord.Type.Screenshots, timeline);
        
        super(timeline, extraArguments);

        this._screenshotsTimeline = timeline;

        this.element.classList.add("screenshots");
        
        this._selectedRecord = null;
        this._selectedRecordNeedsReveal = false;

        this._imageElementForRecord = new WeakMap;
        this._spacerElement = null;
    }

    // Public

    reset()
    {
        super.reset();

        this.selectRecord(null);

        this._imageElementForRecord = new WeakMap;
        this._spacerElement = null;
    }

    // Protected

    get showsFilterBar() { return false; }

    initialLayout()
    {
        super.initialLayout();

        this._scrollView = new WI.ContentView;

        this.addSubview(this._scrollView);
    }

    layout()
    {
        if (this.layoutReason === WI.View.LayoutReason.Resize)
            return;

        super.layout();

        if (this.hidden)
            return;

        let container = this._scrollView.element;

        // Reuse the elements of records that stay visible, instead of removing and re-adding every
        // image on each layout, since capturing lays out on every time update.
        let records = this._visibleRecords();
        let childElements = container.children;

        for (let i = 0; i < records.length; ++i) {
            let record = records[i];
            let imageElement = this._imageElementForRecord.getOrInsertComputed(record, () => {
                let imageElement = document.createElement("img");

                imageElement.hidden = true;
                imageElement.addEventListener("load", (event) => {
                    imageElement.hidden = false;
                }, {once: true});
                imageElement.src = record.imageData;

                imageElement.addEventListener("click", (event) => {
                    this._selectTimelineRecord(record);
                });

                return imageElement;
            });

            if (childElements[i] !== imageElement) {
                container.insertBefore(imageElement, childElements[i] || null);

                // The element left and re-entered the DOM, so it needs to be revealed again.
                if (record === this._selectedRecord)
                    this._selectedRecordNeedsReveal = true;
            }
        }

        // Trim the elements that no longer have a record, keeping the spacer in place, so that a
        // layout that shows the same records does not move any element.
        while (childElements.length > records.length && childElements[records.length] !== this._spacerElement)
            childElements[records.length].remove();

        // Reveal the selection once, when its image was created or re-inserted or when the selection
        // changed, so that capturing does not re-center it on every layout.
        if (this._selectedRecord && this._selectedRecordNeedsReveal) {
            let selectedElement = this._imageElementForRecord.get(this._selectedRecord);
            if (selectedElement && records.includes(this._selectedRecord)) {
                selectedElement.classList.add("selected");

                selectedElement.scrollIntoView({inline: "center"});

                this._selectedRecordNeedsReveal = false;
            }
        }

        if (records.length) {
            if (!this._spacerElement) {
                this._spacerElement = document.createElement("div");
                this._spacerElement.className = "spacer";
            }
            if (this._spacerElement.parentNode !== container)
                container.appendChild(this._spacerElement);
        } else {
            container.removeChildren();
            container.appendChild(WI.createMessageTextView(WI.UIString("No screenshots", "No screenshots @ Screenshots Timeline", "Placeholder text shown when there are no images to display in the Screenshots timeline.")));
        }
    }

    selectRecord(record)
    {
        if (record === this._selectedRecord)
            return;

        if (this._selectedRecord)
            this._imageElementForRecord.get(this._selectedRecord)?.classList.remove("selected");

        this._selectedRecord = record;
        this._selectedRecordNeedsReveal = !!this._selectedRecord;

        if (this._selectedRecord) {
            let element = this._imageElementForRecord.get(this._selectedRecord);
            if (element) {
                element.classList.add("selected");

                if (element.isConnected) {
                    element.scrollIntoView({inline: "center"});
                    this._selectedRecordNeedsReveal = false;
                }
            }
        }
    }

    // Private

    _selectTimelineRecord(record)
    {
        this.dispatchEventToListeners(WI.TimelineView.Event.RecordWasSelected, {record});
    }

    _visibleRecords()
    {
        // `currentTime` is 0 or NaN for an imported recording, or for a capture that starts while
        // the inspector is hidden. See `WI.CPUTimelineView`.
        let visibleEndTime = Math.min(this.endTime, this.currentTime) || this.endTime;

        let records = this._screenshotsTimeline.records;
        let visibleRecords = [];

        for (let i = 0; i < records.length; ++i) {
            let record = records[i];
            if (record.startTime < this.startTime)
                continue;

            if (record.startTime > visibleEndTime)
                break;

            if (!visibleRecords.length && i)
                visibleRecords.push(records[i - 1]);

            visibleRecords.push(record);
        }

        return visibleRecords;
    }
};

WI.ScreenshotsTimelineView.ReferencePage = WI.ReferencePage.TimelinesTab.ScreenshotsTimeline;
