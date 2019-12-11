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

WI.AuditTestGroupContentView = class AuditTestGroupContentView extends WI.AuditTestContentView
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WI.AuditTestGroup || representedObject instanceof WI.AuditTestGroupResult);

        super(representedObject);

        this.element.classList.add("audit-test-group");
        this.element.classList.toggle("contains-test-case", this._subobjects().some((test) => test instanceof WI.AuditTestCase || test instanceof WI.AuditTestCaseResult));
        this.element.classList.toggle("contains-test-group", this._subobjects().some((test) => test instanceof WI.AuditTestGroup || test instanceof WI.AuditTestGroupResult));

        this._levelScopeBar = null;
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        let informationContainer = this.headerView.element.appendChild(document.createElement("div"));
        informationContainer.classList.add("information");

        let nameElement = informationContainer.appendChild(document.createElement("h1"));
        nameElement.textContent = this.representedObject.name;

        if (this.representedObject.description) {
            let descriptionElement = informationContainer.appendChild(document.createElement("p"));
            descriptionElement.textContent = this.representedObject.description;
        }

        this._levelNavigationBar = new WI.NavigationBar(document.createElement("nav"));
        this.headerView.addSubview(this._levelNavigationBar);

        this._percentageContainer = this.headerView.element.appendChild(document.createElement("div"));
        this._percentageContainer.classList.add("percentage-pass");
        this._percentageContainer.hidden = true;

        this._percentageTextElement = document.createElement("span");

        const format = WI.UIString("%s%%", "Percentage (of audits)", "The number of tests that passed expressed as a percentage, followed by a literal %.");
        String.format(format, [this._percentageTextElement], String.standardFormatters, this._percentageContainer, (a, b) => {
            a.append(b);
            return a;
        });
    }

    layout()
    {
        if (this.layoutReason !== WI.View.LayoutReason.Dirty)
            return;

        super.layout();

        let result = this.representedObject.result;
        if (!result) {
            if (this._levelScopeBar) {
                this._levelNavigationBar.removeNavigationItem(this._levelScopeBar);
                this._levelScopeBar = null;
            }

            this._percentageContainer.hidden = true;
            this._percentageTextElement.textContent = "";

            if (this.representedObject.runningState === WI.AuditManager.RunningState.Inactive)
                this.showNoResultPlaceholder();
            else if (this.representedObject.runningState === WI.AuditManager.RunningState.Active)
                this.showRunningPlaceholder();
            else if (this.representedObject.runningState === WI.AuditManager.RunningState.Stopping)
                this.showStoppingPlaceholder();

            return;
        }

        let levelCounts = result.levelCounts;
        let totalCount = Object.values(levelCounts).reduce((accumulator, current) => accumulator + current);
        this._percentageTextElement.textContent = Math.floor(100 * levelCounts[WI.AuditTestCaseResult.Level.Pass] / totalCount);
        this._percentageContainer.hidden = false;

        if (!this._levelScopeBar) {
            let scopeBarItems = [];

            let addScopeBarItem = (level, labelSingular, labelPlural) => {
                let count = levelCounts[level];
                if (isNaN(count) || count <= 0)
                    return;

                let label = (labelPlural && count !== 1) ? labelPlural : labelSingular;
                let scopeBarItem = new WI.ScopeBarItem(level, label.format(count), {
                    className: level,
                    exclusive: false,
                    independent: true,
                });
                scopeBarItem.selected = true;

                scopeBarItem.element.insertBefore(document.createElement("img"), scopeBarItem.element.firstChild);

                scopeBarItems.push(scopeBarItem);
            };

            addScopeBarItem(WI.AuditTestCaseResult.Level.Pass, WI.UIString("%d Passed", "%d Passed (singular)", ""), WI.UIString("%d Passed", "%d Passed (plural)", ""));
            addScopeBarItem(WI.AuditTestCaseResult.Level.Warn, WI.UIString("%d Warning"), WI.UIString("%d Warnings"));
            addScopeBarItem(WI.AuditTestCaseResult.Level.Fail, WI.UIString("%d Failed", "%d Failed (singular)", ""), WI.UIString("%d Failed", "%d Failed (plural)", ""));
            addScopeBarItem(WI.AuditTestCaseResult.Level.Error, WI.UIString("%d Error"), WI.UIString("%d Errors"));
            addScopeBarItem(WI.AuditTestCaseResult.Level.Unsupported, WI.UIString("%d Unsupported", "%d Unsupported (singular)", ""), WI.UIString("%d Unsupported", "%d Unsupported (plural)", ""));

            this._levelScopeBar = new WI.ScopeBar(null, scopeBarItems);
            this._levelScopeBar.addEventListener(WI.ScopeBar.Event.SelectionChanged, this._handleLevelScopeBarSelectionChanged, this);
            this._levelNavigationBar.addNavigationItem(this._levelScopeBar);
        }

        if (this.applyFilter())
            this.hidePlaceholder();
        else
            this.showFilteredPlaceholder();
    }

    shown()
    {
        super.shown();

        if (this.representedObject instanceof WI.AuditTestGroup) {
            this.representedObject.addEventListener(WI.AuditTestBase.Event.Progress, this._handleTestGroupProgress, this);
            this.representedObject.addEventListener(WI.AuditTestBase.Event.Scheduled, this._handleTestGroupScheduled, this);
        }

        for (let subobject of this._subobjects()) {
            if (subobject instanceof WI.AuditTestBase && subobject.disabled)
                continue;

            let view = WI.ContentView.contentViewForRepresentedObject(subobject);
            this.contentView.addSubview(view);
            view.shown();
        }
    }

    hidden()
    {
        for (let view of this.contentView.subviews)
            view.hidden();

        this.contentView.removeAllSubviews();

        super.hidden();
    }

    applyFilter(levels)
    {
        if (this._levelScopeBar && !levels)
            levels = this._levelScopeBar.selectedItems.map((item) => item.id);

        this._updateLevelScopeBar(levels);

        return super.applyFilter(levels);
    }

    resetFilter()
    {
        this._updateLevelScopeBar(Object.values(WI.AuditTestCaseResult.Level));

        super.resetFilter();
    }

    showRunningPlaceholder()
    {
        if (!this.placeholderElement || !this.placeholderElement.__placeholderRunning) {
            this.placeholderElement = WI.createMessageTextView(WI.UIString("Running the \u201C%s\u201D audit").format(this.representedObject.name));
            this.placeholderElement.__placeholderRunning = true;

            this.placeholderElement.__progress = document.createElement("progress");
            this.placeholderElement.__progress.value = 0;
            this.placeholderElement.appendChild(this.placeholderElement.__progress);
        }

        super.showRunningPlaceholder();
    }

    // Private

    _subobjects()
    {
        if (this.representedObject instanceof WI.AuditTestGroup)
            return this.representedObject.tests;

        if (this.representedObject instanceof WI.AuditTestGroupResult)
            return this.representedObject.results;

        console.error("Unknown representedObject", this.representedObject);
        return [];
    }

    _updateLevelScopeBar(levels)
    {
        if (!this._levelScopeBar)
            return;

        for (let item of this._levelScopeBar.items)
            item.selected = levels.includes(item.id);

        for (let view of this.contentView.subviews) {
            if (view instanceof WI.AuditTestGroupContentView)
                view._updateLevelScopeBar(levels);
        }
    }

    _handleTestGroupProgress(event)
    {
        let {index, count} = event.data;
        if (this.placeholderElement && this.placeholderElement.__progress)
            this.placeholderElement.__progress.value = (index + 1) / count;
    }

    _handleTestGroupScheduled(event)
    {
        if (this.placeholderElement && this.placeholderElement.__progress)
            this.placeholderElement.__progress.value = 0;
    }

    _handleLevelScopeBarSelectionChanged(event)
    {
        this.needsLayout();
    }
};
