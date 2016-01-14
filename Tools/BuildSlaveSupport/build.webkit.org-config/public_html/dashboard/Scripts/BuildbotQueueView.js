/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

BuildbotQueueView = function(queues)
{
    QueueView.call(this);

    this.queues = queues || [];

    this.queues.forEach(function(queue) {
        if (this.platform && this.platform != queue.platform)
            throw "A buildbot view may not contain queues for multiple platforms."
        else
            this.platform = queue.platform;
        queue.addEventListener(BuildbotQueue.Event.IterationsAdded, this._queueIterationsAdded, this);
        queue.addEventListener(BuildbotQueue.Event.UnauthorizedAccess, this._unauthorizedAccess, this);
    }.bind(this));

    Dashboard.Repository.OpenSource.trac.addEventListener(Trac.Event.CommitsUpdated, this._newCommitsRecorded, this);
    if (typeof Dashboard.Repository.Internal.trac != "undefined")
        Dashboard.Repository.Internal.trac.addEventListener(Trac.Event.CommitsUpdated, this._newCommitsRecorded, this);
};

BaseObject.addConstructorFunctions(BuildbotQueueView);

BuildbotQueueView.UpdateInterval = 45000; // 45 seconds
BuildbotQueueView.UpdateSoonTimeout = 1000; // 1 second

BuildbotQueueView.prototype = {
    constructor: BuildbotQueueView,
    __proto__: QueueView.prototype,

    _latestProductiveIteration: function(queue)
    {
        if (!queue.iterations.length)
            return null;
        if (queue.iterations[0].productive)
            return queue.iterations[0];
        return queue.iterations[0].previousProductiveIteration;
    },

    _appendUnauthorizedLineView: function(queue)
    {
        console.assert(queue.buildbot.needsAuthentication);
        console.assert(!queue.buildbot.isAuthenticated);
        var javascriptURL = "javascript:buildbots[" + buildbots.indexOf(queue.buildbot) + "].updateQueues(Buildbot.UpdateReason.Reauthenticate)";
        var status = new StatusLineView("unauthorized", StatusLineView.Status.Unauthorized, "", null, javascriptURL);
        this.element.appendChild(status.element);
    },

    _appendPendingRevisionCount: function(queue)
    {
        var latestProductiveIteration = this._latestProductiveIteration(queue);
        if (!latestProductiveIteration)
            return;

        var totalRevisionsBehind = 0;

        // FIXME: To be 100% correct, we should also filter out changes that are ignored by
        // the queue, see _should_file_trigger_build in wkbuild.py.
        var branches = queue.branches;
        for (var i = 0; i < branches.length; ++i) {
            var branch = branches[i];
            var repository = branch.repository;
            var repositoryName = repository.name;
            var trac = repository.trac;
            var latestProductiveRevisionNumber = latestProductiveIteration.revision[repositoryName];
            if (!latestProductiveRevisionNumber)
                continue;
            if (!trac)
                continue;
            if (!trac.latestRecordedRevisionNumber || trac.oldestRecordedRevisionNumber > latestProductiveRevisionNumber) {
                trac.loadMoreHistoricalData();
                return;
            }

            totalRevisionsBehind += trac.commitsOnBranch(branch.name, function(commit) { return commit.revisionNumber > latestProductiveRevisionNumber; }).length;
        }

        if (!totalRevisionsBehind)
            return;

        var messageElement = document.createElement("span"); // We can't just pass text to StatusLineView here, because we need an element that perfectly fits the text for popover positioning.
        messageElement.textContent = totalRevisionsBehind + " " + (totalRevisionsBehind === 1 ? "revision behind" : "revisions behind");
        var status = new StatusLineView(messageElement, StatusLineView.Status.NoBubble);
        this.element.appendChild(status.element);

        new PopoverTracker(messageElement, this._presentPopoverForPendingCommits.bind(this), queue);
    },

    _popoverLinesForCommitRange: function(trac, branchName, firstRevisionNumber, lastRevisionNumber)
    {
        function lineForCommit(trac, commit)
        {
            var result = document.createElement("div");
            result.className = "pending-commit";

            var linkElement = document.createElement("a");
            linkElement.className = "revision";
            linkElement.href = trac.revisionURL(commit.revisionNumber);
            linkElement.target = "_blank";
            linkElement.textContent = "r" + commit.revisionNumber;
            result.appendChild(linkElement);

            var authorElement = document.createElement("span");
            authorElement.className = "author";
            authorElement.textContent = commit.author;
            result.appendChild(authorElement);

            var titleElement = document.createElement("span");
            titleElement.className = "title";
            titleElement.innerHTML = commit.title.innerHTML;
            result.appendChild(titleElement);

            return result;
        }

        console.assert(trac.oldestRecordedRevisionNumber <= firstRevisionNumber);

        // FIXME: To be 100% correct, we should also filter out changes that are ignored by
        // the queue, see _should_file_trigger_build in wkbuild.py.
        var commits = trac.commitsOnBranch(branchName, function(commit) { return commit.revisionNumber >= firstRevisionNumber && commit.revisionNumber <= lastRevisionNumber; });
        return commits.map(function(commit) {
            return lineForCommit(trac, commit);
        }, this).reverse();
    },

    _presentPopoverForPendingCommits: function(element, popover, queue)
    {
        var latestProductiveIteration = this._latestProductiveIteration(queue);
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
            var trac = repository.trac;
            var latestProductiveRevisionNumber = latestProductiveIteration.revision[repositoryName];
            if (!latestProductiveRevisionNumber || !trac.latestRecordedRevisionNumber)
                continue;
            var lines = this._popoverLinesForCommitRange(trac, branch.name, latestProductiveRevisionNumber + 1, trac.latestRecordedRevisionNumber);
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

    _presentPopoverForRevisionRange: function(element, popover, context)
    {
        var content = document.createElement("div");
        content.className = "commit-history-popover";

        // FIXME: Nothing guarantees that Trac has historical data for these revisions.
        var linesForCommits = this._popoverLinesForCommitRange(context.trac, context.branchName, context.firstRevision, context.lastRevision);

        var line = document.createElement("div");
        line.className = "title";

        if (linesForCommits.length) {
            line.textContent = "commits since previous result";
            content.appendChild(line);
            this._addDividerToPopover(content);
        } else {
            line.textContent = "no commits to " + context.branchName + " since previous result";
            content.appendChild(line);
        }

        for (var i = 0; i != linesForCommits.length; ++i)
            content.appendChild(linesForCommits[i]);

        var rect = Dashboard.Rect.rectFromClientRect(element.getBoundingClientRect());
        popover.content = content;
        popover.present(rect, [Dashboard.RectEdge.MIN_Y, Dashboard.RectEdge.MAX_Y, Dashboard.RectEdge.MAX_X, Dashboard.RectEdge.MIN_X]);

        return true;
    },

    _revisionContentWithPopoverForIteration: function(iteration, previousIteration, branch)
    {
        var repository = branch.repository;
        var repositoryName = repository.name;
        console.assert(iteration.revision[repositoryName]);
        var content = document.createElement("span");
        var revision = iteration.revision[repositoryName];
        if (repository.isSVN)
            content.textContent = "r" + revision;
        else if (repository.isGit) {
            // Truncating for display. Git traditionally uses seven characters for a short hash.
            content.textContent = revision.substr(0, 7);
        } else
            console.assert(false, "Should not get here; " + repository.name + " did not specify a known VCS type.");
        content.classList.add("revision-number");

        if (previousIteration) {
            console.assert(previousIteration.revision[repositoryName]);
            var context = {
                trac: repository.trac,
                branchName: branch.name,
                firstRevision: previousIteration.revision[repositoryName] + 1,
                lastRevision: iteration.revision[repositoryName]
            };
            if (context.firstRevision <= context.lastRevision)
                new PopoverTracker(content, this._presentPopoverForRevisionRange.bind(this), context);
        }

        return content;
    },

    _addIterationHeadingToPopover: function(iteration, content, additionalText)
    {
        var title = document.createElement("div");
        title.className = "popover-iteration-heading";

        var queueName = document.createElement("span");
        queueName.className = "queue-name";
        queueName.textContent = iteration.queue.id;
        title.appendChild(queueName);

        var buildbotLink = document.createElement("a");
        buildbotLink.className = "buildbot-link";
        buildbotLink.textContent = "build #" + iteration.id;
        buildbotLink.href = iteration.queue.buildbot.buildPageURLForIteration(iteration);
        buildbotLink.target = "_blank";
        title.appendChild(buildbotLink);

        if (additionalText) {
            var additionalTextElement = document.createElement("span");
            additionalTextElement.className = "additional-text";
            additionalTextElement.textContent = additionalText;
            title.appendChild(additionalTextElement);
        }

        content.appendChild(title);
    },

    _addDividerToPopover: function(content)
    {
        var divider = document.createElement("div");
        divider.className = "divider";
        content.appendChild(divider);
    },

    revisionContentForIteration: function(iteration, previousDisplayedIteration)
    {
        var fragment = document.createDocumentFragment();
        var shouldAddPlusSign = false;
        var branches = iteration.queue.branches;
        for (var i = 0; i < branches.length; ++i) {
            var branch = branches[i];
            if (!iteration.revision[branch.repository.name])
                continue;
            var content = this._revisionContentWithPopoverForIteration(iteration, previousDisplayedIteration, branch);
            if (shouldAddPlusSign)
                fragment.appendChild(document.createTextNode(" \uff0b "));
            fragment.appendChild(content);
            shouldAddPlusSign = true;
        }
        console.assert(fragment.childNodes.length);
        return fragment;
    },

    appendBuildStyle: function(queues, defaultLabel, appendFunction)
    {
        queues.forEach(function(queue) {
            var releaseLabel = document.createElement("a");
            releaseLabel.classList.add("queueLabel");
            releaseLabel.textContent = queue.heading ? queue.heading : defaultLabel;
            releaseLabel.href = queue.overviewURL;
            releaseLabel.target = "_blank";
            this.element.appendChild(releaseLabel);

            appendFunction.call(this, queue);
        }.bind(this));
    },

    _updateQueues: function()
    {
        this.queues.forEach(function(queue) { queue.update(); });
    },

    _queueIterationsAdded: function(event)
    {
        this.updateSoon();

        event.data.addedIterations.forEach(function(iteration) {
            iteration.addEventListener(BuildbotIteration.Event.Updated, this._iterationUpdated, this);
            iteration.addEventListener(BuildbotIteration.Event.UnauthorizedAccess, this._unauthorizedAccess, this);
        }.bind(this));
    },

    _iterationUpdated: function(event)
    {
        this.updateSoon();
    },
    
    _newCommitsRecorded: function(event)
    {
        this.updateSoon();
    },

    _unauthorizedAccess: function(event)
    {
        this.updateSoon();
    }

};
