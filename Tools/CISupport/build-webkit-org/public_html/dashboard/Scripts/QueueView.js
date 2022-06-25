/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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

QueueView = function()
{
    BaseObject.call(this);

    this.element = document.createElement("div");
    this.element.classList.add("queue-view");
    this.element.__queueView = this;

    this.updateTimer = null;
    setTimeout(this._updateHiddenState.bind(this), 0); // Lets subclass constructor finish before calling _updateHiddenState.
    settings.addSettingListener("hiddenPlatformFamilies", this._updateHiddenState.bind(this));
};

BaseObject.addConstructorFunctions(QueueView);

QueueView.UpdateInterval = 45000; // 45 seconds
QueueView.UpdateSoonTimeout = 1000; // 1 second
QueueView.MAX_POPOVER = 20;

QueueView.prototype = {
    constructor: QueueView,
    __proto__: BaseObject.prototype,

    updateSoon: function()
    {
        if (this._updateTimeout)
            return;
        this._updateTimeout = setTimeout(this.update.bind(this), QueueView.UpdateSoonTimeout);
    },

    update: function()
    {
        if (this._updateTimeout) {
            clearTimeout(this._updateTimeout);
            delete this._updateTimeout;
        }

        // Implemented by subclasses.
    },

    _updateHiddenState: function()
    {
        if (!settings.available())
            return;

        var hiddenPlatformFamilies = settings.getObject("hiddenPlatformFamilies");
        var wasHidden = !this.updateTimer;
        var isHidden = hiddenPlatformFamilies && hiddenPlatformFamilies.contains(settings.parsePlatformFamily(this.platform));

        if (wasHidden && !isHidden) {
            this._updateQueues();
            this.updateTimer = setInterval(this._updateQueues.bind(this), QueueView.UpdateInterval);
        } else if (!wasHidden && isHidden) {
            clearInterval(this.updateTimer);
            this.updateTimer = null;
        }
    },

    addLinkToRow: function(rowElement, className, text, url)
    {   
        var linkElement = document.createElement("a");
        linkElement.className = className;
        linkElement.textContent = text;
        linkElement.href = url;
        linkElement.target = "_blank";
        rowElement.appendChild(linkElement);
    },  

    addTextToRow: function(rowElement, className, text)
    {   
        var spanElement = document.createElement("span");
        spanElement.className = className;
        spanElement.textContent = text;
        rowElement.appendChild(spanElement);
    },

    _addDividerToPopover: function(content)
    {
        var divider = document.createElement("div");
        divider.className = "divider";
        content.appendChild(divider);
    },

    _appendPendingRevisionCount: function(queue, latestIterationGetter)
    {
        var latestProductiveIteration = latestIterationGetter();
        if (!latestProductiveIteration)
            return;

        var totalRevisionsBehind = 0;

        // FIXME: To be 100% correct, we should also filter out changes that are ignored by
        // the queue, see _should_file_trigger_build in wkbuild.py.
        var branches = queue.branches;
        for (var i = 0; i < branches.length; ++i) {
            var branch = branches[i];
            if (branch.name === 'trunk')
                branch.name = 'main';
            var repository = branch.repository;
            var repositoryName = repository.name;
            var commits = repository.commits;
            var latestProductiveIdentifier = latestProductiveIteration.revision[repositoryName];
            if (!latestProductiveIdentifier)
                continue;
            if (!commits)
                continue;
            var head = commits.heads[branch.name];
            if (!head) {
                commits.heads[branch.name] = null;
                continue;
            }
            totalRevisionsBehind = commits.branchPosition(commits.heads[branch.name].identifier) - commits.branchPosition(latestProductiveIdentifier);
        }

        if (!totalRevisionsBehind)
            return;

        commits.fetch(branch.name, Math.min(totalRevisionsBehind, QueueView.MAX_POPOVER));
        var messageElement = document.createElement("span"); // We can't just pass text to StatusLineView here, because we need an element that perfectly fits the text for popover positioning.
        messageElement.textContent = totalRevisionsBehind + " " + (totalRevisionsBehind === 1 ? "commit behind" : "commits behind");
        var status = new StatusLineView(messageElement, StatusLineView.Status.NoBubble);
        this.element.appendChild(status.element);

        new PopoverTracker(messageElement, this._presentPopoverForPendingCommits.bind(this, latestIterationGetter), queue);
    },

    _popoverLinesForCommitRange: function(commits, branch, commitList)
    {
        function lineForCommit(commits, commit)
        {
            var result = document.createElement("div");
            result.className = "pending-commit";

            var linkElement = document.createElement("a");
            linkElement.className = "revision";
            linkElement.href = commits.urlFor(commit.identifier);
            linkElement.target = "_blank";
            linkElement.textContent = this._formatRevisionForDisplay(commit.identifier, branch.repository);
            result.appendChild(linkElement);

            var authorElement = document.createElement("span");
            authorElement.className = "author";
            authorElement.textContent = commit.author ? commit.author.name : '';
            result.appendChild(authorElement);

            var titleElement = document.createElement("span");
            titleElement.className = "title";
            titleElement.innerHTML = commit.message ? commit.message.split('\n')[0] : '';
            result.appendChild(titleElement);

            return result;
        }

        return commitList.map(function(commit) {
            return lineForCommit.call(this, commits, commit);
        }, this);
    },

    _presentPopoverForPendingCommits: function(latestIterationGetter, element, popover, queue)
    {
        var latestProductiveIteration = latestIterationGetter();
        if (!latestProductiveIteration)
            return false;

        var content = document.createElement("div");
        content.className = "commit-history-popover";

        var shouldAddDivider = false;
        var branches = queue.branches;
        for (var i = 0; i < branches.length; ++i) {
            var branch = branches[i];
            var repository = branch.repository;
            var repositoryName = repository.name;
            var commits = repository.commits;
            if (!commits.heads[branch.name])
                continue;

            var commitList = [];
            var latestProductiveIdentifier = latestProductiveIteration.revision[repositoryName];
            for (const identifier of commits.lastNIdentifiers(branch.name, QueueView.MAX_POPOVER)) {
                if (identifier == latestProductiveIdentifier)
                    break;
                if (commits.commits[identifier])
                    commitList.push(commits.commits[identifier]);
            }
            if (!commitList.length)
                continue
            var lines = this._popoverLinesForCommitRange(commits, branch, commitList);
            var length = lines.length;
            if (length && shouldAddDivider)
                this._addDividerToPopover(content);
            for (var j = 0; j < length; ++j)
                content.appendChild(lines[j]);
            shouldAddDivider = shouldAddDivider || length > 0;
        }

        var rect = Dashboard.Rect.rectFromClientRect(element.getBoundingClientRect());
        popover.content = content;
        popover.present(rect, [Dashboard.RectEdge.MIN_Y, Dashboard.RectEdge.MAX_Y, Dashboard.RectEdge.MAX_X, Dashboard.RectEdge.MIN_X]);

        return true;
    },

    _formatRevisionForDisplay: function(revision, repository)
    {
        console.assert(repository.isSVN || repository.isGit, "Should not get here; " + repository.name + " did not specify a known VCS type.");
        if (repository.isSVN)
            return "r" + revision;
        if (revision.includes('@'))
            return revision;
        // Truncating for display. Git traditionally uses seven characters for a short hash.
        return revision.substr(0, 7);
    },

    _readableTimeString: function(seconds)
    {
        var secondsInHour = 60 * 60;
        var hours = Math.floor(seconds / secondsInHour);
        var minutes = Math.floor((seconds - hours * secondsInHour) / 60);
        var hoursPart = "";
        if (hours === 1)
            hoursPart = "1\xa0hour and ";
        else if (hours > 0)
            hoursPart = hours + "\xa0hours and ";
        if (!minutes)
            return "less than a minute";
        if (minutes === 1)
            return hoursPart + "1\xa0minute";
        return hoursPart + minutes + "\xa0minutes";
    }
};
