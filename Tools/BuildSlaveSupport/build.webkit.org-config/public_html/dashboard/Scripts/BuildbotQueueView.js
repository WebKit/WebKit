/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

    var sortedRepositories = Dashboard.sortedRepositories;
    for (var i = 0; i < sortedRepositories.length; i++) {
        var trac = sortedRepositories[i].trac;
        if (trac)
            trac.addEventListener(Trac.Event.CommitsUpdated, this._newCommitsRecorded, this);
    }
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

    _createLoadingIndicator: function(iteration, heading) {
        var content = document.createElement("div");
        content.className = "test-results-popover";

        this._addIterationHeadingToPopover(iteration, content, heading);
        this._addDividerToPopover(content);

        var loadingIndicator = document.createElement("div");
        loadingIndicator.className = "loading-indicator";
        loadingIndicator.textContent = "Loading\u2026";
        content.appendChild(loadingIndicator);

        return content;
    },

    // Work around bug 80159: -webkit-user-select:none not respected when copying content.
    // We set clipboard data manually, temporarily making non-selectable content hidden
    // to easily get accurate selection text.
    _onPopoverCopy: function(event) {
        var iterator = document.createNodeIterator(
            event.currentTarget,
            NodeFilter.SHOW_ELEMENT,
            {
                acceptNode: function(element) {
                    if (window.getComputedStyle(element).webkitUserSelect !== "none")
                        return NodeFilter.FILTER_ACCEPT;
                    return NodeFilter.FILTER_SKIP;
                }
            }
        );

        while ((node = iterator.nextNode()))
            node.style.visibility = "visible";

        event.currentTarget.style.visibility = "hidden";
        event.clipboardData.setData('text', window.getSelection());
        event.currentTarget.style.visibility = "";
        return false;
    },

    _popoverContentForJavaScriptCoreTestRegressions: function(iteration, testName)
    {
        var content = document.createElement("div");
        content.className = "test-results-popover";

        this._addIterationHeadingToPopover(iteration, content, "javascriptcore test failures", iteration.queue.buildbot.javaScriptCoreTestStdioUrlForIteration(iteration, testName));
        this._addDividerToPopover(content);

        if (!iteration.javaScriptCoreTestResults.regressions) {
            var message = document.createElement("div");
            message.className = "loading-failure";
            message.textContent = "Test results couldn\u2019t be loaded";
            content.appendChild(message);
            return content;
        }

        var regressions = iteration.javaScriptCoreTestResults.regressions;

        for (var i = 0, end = regressions.length; i != end; ++i) {
            var test = regressions[i];
            var rowElement = document.createElement("div");
            var testPathElement = document.createElement("span");
            testPathElement.className = "test-path";
            testPathElement.textContent = test;
            rowElement.appendChild(testPathElement);
            content.appendChild(rowElement);
        }

        content.oncopy = this._onPopoverCopy;
        return content;
    },

    _presentPopoverForJavaScriptCoreTestRegressions: function(testName, element, popover, iteration)
    {
        if (iteration.javaScriptCoreTestResults.regressions)
            var content = this._popoverContentForJavaScriptCoreTestRegressions(iteration, testName);
        else {
            var content = this._createLoadingIndicator(iteration, "javascriptcore test failures");
            iteration.loadJavaScriptCoreTestResults(testName, function() {
                popover.content = this._popoverContentForJavaScriptCoreTestRegressions(iteration, testName);
            }.bind(this));
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
        var linesForCommits = this._popoverLinesForCommitRange(context.trac, context.branch, context.firstRevision, context.lastRevision);

        var line = document.createElement("div");
        line.className = "title";

        if (linesForCommits.length) {
            line.textContent = "commits since previous result";
            content.appendChild(line);
            this._addDividerToPopover(content);
        } else {
            line.textContent = "no commits to " + context.branch.name + " since previous result";
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
        var trac = repository.trac;
        var content = document.createElement("span");
        content.textContent = this._formatRevisionForDisplay(iteration.revision[repositoryName], repository);
        content.classList.add("revision-number");

        if (!previousIteration)
            return content;

        var previousIterationRevision = previousIteration.revision[repositoryName];
        console.assert(previousIterationRevision);
        var previousIterationRevisionIndex = trac.indexOfRevision(previousIterationRevision);
        if (previousIterationRevisionIndex === -1)
            return content;

        var firstRevision = trac.nextRevision(branch.name, previousIterationRevision);
        if (firstRevision === Trac.NO_MORE_REVISIONS)
            return content;
        var firstRevisionIndex = trac.indexOfRevision(firstRevision);
        console.assert(firstRevisionIndex !== -1);

        var lastRevision = iteration.revision[repositoryName];
        var lastRevisionIndex = trac.indexOfRevision(lastRevision);
        if (lastRevisionIndex === -1)
            return content;

        if (firstRevisionIndex > lastRevisionIndex)
            return content;

        var context = {
            trac: trac,
            branch: branch,
            firstRevision: firstRevision,
            lastRevision: lastRevision,
        };
        new PopoverTracker(content, this._presentPopoverForRevisionRange.bind(this), context);

        return content;
    },

    _addIterationHeadingToPopover: function(iteration, content, additionalText, additionalTextTarget)
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
            var elementType = additionalTextTarget ? "a" : "span";
            var additionalTextElement = document.createElement(elementType);
            additionalTextElement.className = "additional-text";
            additionalTextElement.textContent = additionalText;
            if (additionalTextTarget) {
                additionalTextElement.href = additionalTextTarget;
                additionalTextElement.target = "_blank";
            }
            title.appendChild(additionalTextElement);
        }

        content.appendChild(title);
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
        return fragment;
    },

    _testStepFailureDescription: function(failedStep)
    {
        if (!failedStep.failureCount)
            return BuildbotIteration.TestSteps[failedStep.name] + " failed";
        if (failedStep.failureCount === 1)
            return BuildbotIteration.TestSteps[failedStep.name] + " failure";
        return BuildbotIteration.TestSteps[failedStep.name] + " failures";
    },

    _testStepFailureDescriptionWithCount: function(failedStep)
    {
        if (!failedStep.failureCount)
            return this._testStepFailureDescription(failedStep);
        if (failedStep.tooManyFailures) {
            // E.g. "50+ layout test failures", preventing line breaks around the "+".
            return failedStep.failureCount + "\ufeff\uff0b\u00a0" + this._testStepFailureDescription(failedStep);
        }
        // E.g. "1 layout test failure", preventing line break after the number.
        return failedStep.failureCount + "\u00a0" + this._testStepFailureDescription(failedStep);
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
