/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.AuditTestContentView = class AuditTestContentView extends WI.ContentView
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WI.AuditTestBase || representedObject instanceof WI.AuditTestResultBase);

        super(representedObject);

        // This class should not be instantiated directly. Create a concrete subclass instead.
        console.assert(this.constructor !== WI.AuditTestContentView && this instanceof WI.AuditTestContentView);

        this.element.classList.add("audit-test");

        this._exportButtonNavigationItem = new WI.ButtonNavigationItem("audit-export", WI.UIString("Export"), "Images/Export.svg", 15, 15);
        this._exportButtonNavigationItem.tooltip = WI.UIString("Export result (%s)").format(WI.saveKeyboardShortcut.displayName);
        this._exportButtonNavigationItem.buttonStyle = WI.ButtonNavigationItem.Style.ImageAndText;
        this._exportButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._exportButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleExportButtonNavigationItemClicked, this);
        this._updateExportButtonNavigationItemState();

        this._headerView = new WI.View(document.createElement("header"));
        this._contentView = new WI.View(document.createElement("section"));
        this._placeholderElement = null;

        this._shownResult = null;
    }

    // Public

    get navigationItems()
    {
        return [this._exportButtonNavigationItem];
    }

    // Protected

    get headerView() { return this._headerView; }
    get contentView() { return this._contentView; }

    get supportsSave()
    {
        return !!this.representedObject.result;
    }

    get saveData()
    {
        return {customSaveHandler: () => { this._exportResult(); }};
    }

    get result()
    {
        if (this.representedObject instanceof WI.AuditTestBase)
            return this.representedObject;
        return this.representedObject.result;
    }

    initialLayout()
    {
        super.initialLayout();

        this.addSubview(this._headerView);
        this.addSubview(this._contentView);
    }

    layout()
    {
        super.layout();

        this.hidePlaceholder();
        this._updateExportButtonNavigationItemState();
    }

    shown()
    {
        super.shown();

        if (this.representedObject instanceof WI.AuditTestBase) {
            this.representedObject.addEventListener(WI.AuditTestBase.Event.Completed, this._handleTestChanged, this);
            this.representedObject.addEventListener(WI.AuditTestBase.Event.Progress, this._handleTestChanged, this);
            this.representedObject.addEventListener(WI.AuditTestBase.Event.ResultChanged, this.handleResultChanged, this);
            this.representedObject.addEventListener(WI.AuditTestBase.Event.Scheduled, this._handleTestChanged, this);
            this.representedObject.addEventListener(WI.AuditTestBase.Event.Stopping, this._handleTestChanged, this);
        }
    }

    hidden()
    {
        if (this.representedObject instanceof WI.AuditTestBase)
            this.representedObject.removeEventListener(null, null, this);

        super.hidden();
    }

    handleResultChanged(event)
    {
        // Overridden by sub-classes.

        this.needsLayout();
    }

    get placeholderElement()
    {
        return this._placeholderElement;
    }

    set placeholderElement(placeholderElement)
    {
        this.hidePlaceholder();

        this._placeholderElement = placeholderElement;
    }

    showRunningPlaceholder()
    {
        // Overridden by sub-classes.

        console.assert(this.placeholderElement);

        this._showPlaceholder();
    }

    showStoppingPlaceholder()
    {
        if (!this.placeholderElement || !this.placeholderElement.__placeholderStopping) {
            this.placeholderElement = WI.createMessageTextView(WI.UIString("Stopping the \u201C%s\u201D audit").format(this.representedObject.name));
            this.placeholderElement.__placeholderStopping = true;

            let spinner = new WI.IndeterminateProgressSpinner;
            this.placeholderElement.appendChild(spinner.element);
        }

        this._showPlaceholder();
    }

    showNoResultPlaceholder()
    {
        if (!this.placeholderElement || !this.placeholderElement.__placeholderNoResult) {
            this.placeholderElement = WI.createMessageTextView(WI.UIString("No Result"));
            this.placeholderElement.__placeholderNoResult = true;

            let startNavigationItem = new WI.ButtonNavigationItem("run-audit", WI.UIString("Start"), "Images/AuditStart.svg", 15, 15);
            startNavigationItem.buttonStyle = WI.ButtonNavigationItem.Style.ImageAndText;
            startNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, () => {
                WI.auditManager.start([this.representedObject]);
            });

            let importHelpElement = WI.createNavigationItemHelp(WI.UIString("Press %s to start running the audit"), startNavigationItem);
            this.placeholderElement.appendChild(importHelpElement);
        }

        this._showPlaceholder();
    }

    showNoResultDataPlaceholder()
    {
        if (!this.placeholderElement || !this.placeholderElement.__placeholderNoResultData) {
            let result = this.representedObject.result;
            if (!result) {
                this.showNoResultPlaceholder();
                return;
            }

            let message = null;
            if (result.didError)
                message = WI.UIString("The \u201C%s\u201D audit threw an error");
            else if (result.didFail)
                message = WI.UIString("The \u201C%s\u201D audit failed");
            else if (result.didWarn)
                message = WI.UIString("The \u201C%s\u201D audit resulted in a warning");
            else if (result.didPass)
                message = WI.UIString("The \u201C%s\u201D audit passed");
            else if (result.unsupported)
                message = WI.UIString("The \u201C%s\u201D audit is unsupported");
            else {
                console.error("Unknown result", result);
                return;
            }

            this.placeholderElement = WI.createMessageTextView(message.format(this.representedObject.name), result.didError);
            this.placeholderElement.__placeholderNoResultData = true;
        }

        this._showPlaceholder();
    }

    showFilteredPlaceholder()
    {
        if (!this.placeholderElement || !this.placeholderElement.__placeholderFiltered) {
            this.placeholderElement = WI.createMessageTextView(WI.UIString("No Filter Results"));
            this.placeholderElement.__placeholderFiltered = true;

            let buttonElement = this.placeholderElement.appendChild(document.createElement("button"));
            buttonElement.textContent = WI.UIString("Clear Filters");
            buttonElement.addEventListener("click", () => {
                this.resetFilter();
                this.needsLayout();
            });
        }

        this._showPlaceholder();
    }

    hidePlaceholder()
    {
        this.element.classList.remove("showing-placeholder");
        if (this.placeholderElement)
            this.placeholderElement.remove();
    }

    applyFilter(levels)
    {
        let hasMatch = false;
        for (let view of this.contentView.subviews) {
            let matches = view.applyFilter(levels);
            view.element.classList.toggle("filtered", !matches);

            if (matches)
                hasMatch = true;
        }

        this.element.classList.toggle("no-matches", !hasMatch);

        if (!Array.isArray(levels))
            return true;

        let result = this.representedObject.result;
        if (!result)
            return false;

        if ((levels.includes(WI.AuditTestCaseResult.Level.Error) && result.didError)
            || (levels.includes(WI.AuditTestCaseResult.Level.Fail) && result.didFail)
            || (levels.includes(WI.AuditTestCaseResult.Level.Warn) && result.didWarn)
            || (levels.includes(WI.AuditTestCaseResult.Level.Pass) && result.didPass)
            || (levels.includes(WI.AuditTestCaseResult.Level.Unsupported) && result.unsupported)) {
            return true;
        }

        return false;
    }

    resetFilter()
    {
        for (let view of this.contentView.subviews)
            view.element.classList.remove("filtered");

        this.element.classList.remove("no-matches");
    }

    // Private

    _exportResult()
    {
        WI.auditManager.export(this.representedObject.result);
    }

    _updateExportButtonNavigationItemState()
    {
        this._exportButtonNavigationItem.enabled = !!this.representedObject.result;
    }

    _showPlaceholder()
    {
        this.element.classList.add("showing-placeholder");
        this.contentView.element.appendChild(this.placeholderElement);
    }

    _handleExportButtonNavigationItemClicked(event)
    {
        this._exportResult();
    }

    _handleTestChanged(event)
    {
        this.needsLayout();
    }
};
