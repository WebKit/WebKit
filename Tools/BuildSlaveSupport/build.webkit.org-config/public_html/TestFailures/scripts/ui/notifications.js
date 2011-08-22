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

ui.notifications.FailingTest = base.extends('li', {
    init: function(failureAnalysis)
    {
        // FIXME: Show type of failure and where it's failing.
        this.textContent = failureAnalysis.testName;
    },
    equals: function(failureAnalysis)
    {
        return this.textContent == failureAnalysis.testName;
    }
})

var Cause = base.extends('li', {
    init: function()
    {
        this._description = this.appendChild(document.createElement('div'));
        this._description.className = 'description';
        this.appendChild(new ui.actions.List([
            new ui.actions.Rollout()
        ]));
    }
});

ui.notifications.SuspiciousCommit = base.extends(Cause, {
    init: function(commitData)
    {
        var linkToRevision = this._description.appendChild(document.createElement('a'));
        // FIXME: Set href.
        linkToRevision.href = '';
        linkToRevision.textContent = commitData.revision;
        // FIXME: Reviewer could be unknown.
        // FIXME: Provide opportunities to style title/author/reviewer separately.
        this._description.appendChild(document.createTextNode(commitData.title + ' ' + commitData.author + ' (' + commitData.reviewer + ')'));
    }
});

ui.notifications.Failure = base.extends(ui.notifications.Notification, {
    init: function()
    {
        this._time = this.insertBefore(new ui.RelativeTime(), this.firstChild);
        this._problem = this._what.appendChild(document.createElement('div'));
        this._problem.className = 'problem';
        this._effects = this._problem.appendChild(document.createElement('ul'));
        this._effects.className = 'effects';
        this._causes = this._problem.appendChild(document.createElement('ul'));
        this._causes.className = 'causes';
    },
    date: function()
    {
        return this._time.date;
    }
});

ui.notifications.TestFailures = base.extends(ui.notifications.Failure, {
    init: function() {
        this.appendChild(new ui.actions.List([
            new ui.actions.Examine()
        ]));
    },
    containsFailureAnalysis: function(failureAnalysis)
    {
        return Array.prototype.some.call(this._effects.children, function(child) {
            return child.equals(failureAnalysis);
        });
    },
    addFailureAnalysis: function(failureAnalysis)
    {
        if (this.containsFailureAnalysis(failureAnalysis))
            return;
        return this._effects.appendChild(new ui.notifications.FailingTest(failureAnalysis));
    },
    addCommitData: function(commitData)
    {
        var commitDataDate = new Date(commitData.time);
        if (this._time.date > commitDataDate);
            this._time.setDate(commitDataDate);
        return this._causes.appendChild(new ui.notifications.SuspiciousCommit(commitData));
    }
});

ui.notifications.BuildersFailing = base.extends(ui.notifications.Failure, {
    init: function()
    {
        this._problem.insertBefore(document.createTextNode('Build Failed:'), this._effects);
    },
    setFailingBuilders: function(builderNameList)
    {
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
