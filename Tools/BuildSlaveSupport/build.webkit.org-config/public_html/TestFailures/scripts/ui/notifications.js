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
    push: function(notification)
    {
        this.insertBefore(notification, this.firstChild);
        return notification;
    }
});

var Notification = base.extends('li', {
    init: function()
    {
        this._what = this.appendChild(document.createElement('div'));
        this._what.className = 'what';
        $(this).hide().fadeIn('fast');
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

ui.notifications.Info = base.extends(Notification, {
    init: function(message)
    {
        this._what.textContent = message;
    }
});

var Time = base.extends('time', {
    init: function()
    {
        this.updateTime(new Date());
    },
    updateTime: function(time)
    {
        this.textContent = base.relativizeTime(time);
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
        var actions = this.appendChild(document.createElement('ul'));
        actions.className = 'actions';
        // FIXME: Actions should do something.
        actions.appendChild(document.createElement('li')).innerHTML = '<button>Roll out</button>';
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

ui.notifications.TestFailures = base.extends(Notification, {
    init: function()
    {
        this._time = this.insertBefore(new Time(), this.firstChild);
        var problem = this._what.appendChild(document.createElement('div'));
        problem.className = 'problem';
        this._tests = problem.appendChild(document.createElement('ul'));
        this._tests.className = 'effects';
        this._causes = problem.appendChild(document.createElement('ul'));
        this._causes.className = 'causes';
    },
    containsFailureAnalysis: function(failureAnalysis)
    {
        return Array.prototype.some.call(this._tests.children, function(child) {
            return child.equals(failureAnalysis);
        });
    },
    addFailureAnalysis: function(failureAnalysis)
    {
        if (this.containsFailureAnalysis(failureAnalysis))
            return;
        // FIXME: Retrieve date from failureAnalysis and set this._time.
        // FIXME: Add in order by time.
        return this._tests.appendChild(new ui.notifications.FailingTest(failureAnalysis));
    },
    addCommitData: function(commitData)
    {
        return this._causes.appendChild(new ui.notifications.SuspiciousCommit(commitData));
    }
});

})();
