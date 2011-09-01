/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

var ui = ui || {};
ui.notifications = ui.notifications || {};

(function(){

var kMaxTestsPerGroup = 3;

ui.notifications.Stream = base.extends('ol', {
    init: function()
    {
        this.className = 'notifications';
    },
    add: function(notification)
    {
        var insertBefore = null;
        Array.prototype.some.call(this.children, function(existingNotification) {
            if (existingNotification.index() < notification.index()) {
                insertBefore = existingNotification;
                return true;
            }
        });
        this.insertBefore(notification, insertBefore);
        return notification;
    }
});

ui.notifications.Notification = base.extends('li', {
    init: function()
    {
        this._how = this.appendChild(document.createElement('div'));
        this._how.className = 'how';
        this._what = this.appendChild(document.createElement('div'));
        this._what.className = 'what';
        this._index = 0;
        $(this).hide().fadeIn('fast');
    },
    index: function()
    {
        return this._index;
    },
    setIndex: function(index)
    {
        this._index = index;
    },
    dismiss: function()
    {
        // FIXME: These fade in/out effects are lame.
        $(this).fadeOut(function()
        {
            this.parentNode && this.parentNode.removeChild(this);
        });
    }
});

ui.notifications.Info = base.extends(ui.notifications.Notification, {
    init: function(message)
    {
        this._what.textContent = message;
    }
});

ui.notifications.FailingTestGroup = base.extends('li', {
    init: function(testGroup)
    {
        this.textContent = testGroup;
    }
});

var Cause = base.extends('li', {
    init: function()
    {
        this._description = this.appendChild(document.createElement('div'));
        this._description.className = 'description';
        this.appendChild(new ui.actions.List([
            new ui.actions.Blame(),
            new ui.actions.Rollout(),
        ]));
    }
});

ui.notifications.SuspiciousCommit = base.extends(Cause, {
    init: function(commitData)
    {
        this._revision = commitData.revision;
        var linkToRevision = this._description.appendChild(document.createElement('a'));
        linkToRevision.href = trac.changesetURL(commitData.revision);
        linkToRevision.target = '_blank';
        linkToRevision.textContent = commitData.revision;
        this._addDescriptionPart('summary', commitData);
        this._addDescriptionPart('author', commitData);
        this._addDescriptionPart('reviewer', commitData);
    },
    hasRevision: function(revision)
    {
        return this._revision == revision;
    },
    _addDescriptionPart: function(part, commitData)
    {
        var content = commitData[part];
        if (!content)
            return;

        var span = this._description.appendChild(document.createElement('span'));
        span.className = part;
        span.textContent = content;
    }
});

ui.notifications.Failure = base.extends(ui.notifications.Notification, {
    init: function()
    {
        this._time = this._how.appendChild(new ui.RelativeTime());
        this._where = this._how.appendChild(new ui.failures.FailureGrid());
        this._problem = this._what.appendChild(document.createElement('div'));
        this._problem.className = 'problem';
        this._effects = this._problem.appendChild(document.createElement('ul'));
        this._effects.className = 'effects';
        this._causes = this._what.appendChild(document.createElement('ul'));
        this._causes.className = 'causes';
    },
    date: function()
    {
        return this._time.date;
    }
});

ui.notifications.TestsFailing = base.extends(ui.notifications.Failure, {
    init: function() {
        // FIXME: Convert actions to a link from test!
        this._problem.appendChild(new ui.actions.List([
            new ui.actions.Examine().makeDefault(),
            new ui.actions.Rebaseline(),
        ]));
        this._testNameList = [];
        this._commitDataPinned = false;
    },
    testNameList: function()
    {
        return this._testNameList;
    },
    containsFailureAnalysis: function(failureAnalysis)
    {
        return this._testNameList.indexOf(failureAnalysis.testName) != -1;
    },
    updateBuilderResults: function(resultNodesByBuilder)
    {
        this._where.add(resultNodesByBuilder);
    },
    addFailureAnalysis: function(failureAnalysis)
    {
        if (this.containsFailureAnalysis(failureAnalysis))
            return;
        this._testNameList.push(failureAnalysis.testName);
        this.updateBuilderResults(failureAnalysis.resultNodesByBuilder);
        $(this._effects).empty();
        this._forEachTestGroup(function (testGroup) {
            this._effects.appendChild(new ui.notifications.FailingTestGroup(testGroup))
        }.bind(this));
    },
    pinToCommitData: function(commitData)
    {
        if (this._commitDataPinned)
            return;
        this._commitDataPinned = true;
        $(this._causes).children().each(function() {
            if (this.hasRevision(commitData.revision))
                return;
            $(this).detach();
        });
    },
    addCommitData: function(commitData)
    {
        if (this._commitDataPinned)
            return null;
        var commitDataDate = new Date(commitData.time);
        if (this._time.date > commitDataDate); {
            this.setIndex(commitDataDate.getTime());
            this._time.setDate(commitDataDate);
        }
        return this._causes.appendChild(new ui.notifications.SuspiciousCommit(commitData));
    },
    _forEachTestGroup: function(callback)
    {
        var testsByDirectory = {};
        this._testNameList.forEach(function(testName) {
            var directory = base.dirName(testName);
            testsByDirectory[directory] = testsByDirectory[directory] || [];
            testsByDirectory[directory].push(testName);
        });
        var individualTests = [];
        Object.keys(testsByDirectory).forEach(function(directory) {
            var testsInDirectory = testsByDirectory[directory];
            var count = testsInDirectory.length;
            if (count <= kMaxTestsPerGroup) {
                individualTests = individualTests.concat(testsInDirectory);
                return;
            }
            callback(directory + ' (' + count + ' tests)');
        });
        individualTests.forEach(callback);
    }
});

ui.notifications.BuildersFailing = base.extends(ui.notifications.Failure, {
    init: function()
    {
        this._problem.insertBefore(document.createTextNode('Build Failed:'), this._problem.firstChild);
        $(this._where).detach();
    },
    setFailingBuilders: function(builderNameList)
    {
        // FIXME: Populate this._where with failing builders.
        $(this._effects).empty().append(builderNameList.map(function(builderName) {
            var effect = document.createElement('li');
            effect.className = 'builder-name';
            var link = effect.appendChild(document.createElement('a'));
            link.target = '_blank';
            link.href = ui.displayURLForBuilder(builderName);
            link.textContent = ui.displayNameForBuilder(builderName);
            return effect;
        }));
    }
});

})();
