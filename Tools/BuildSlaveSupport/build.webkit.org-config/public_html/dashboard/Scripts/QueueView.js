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

QueueView = function()
{
    BaseObject.call(this);

    this.element = document.createElement("div");
    this.element.classList.add("queue-view");
    this.element.__queueView = this;

    this.updateTimer = null;
    this._updateHiddenState();
    settings.addSettingListener("hiddenPlatforms", this._updateHiddenState.bind(this));
};

BaseObject.addConstructorFunctions(QueueView);

QueueView.UpdateInterval = 45000; // 45 seconds
QueueView.UpdateSoonTimeout = 1000; // 1 second

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

        var hiddenPlatforms = settings.getObject("hiddenPlatforms");
        var wasHidden = !this.updateTimer;
        var isHidden = hiddenPlatforms && hiddenPlatforms.contains(this.platform);

        if (wasHidden && !isHidden)
            this.updateTimer = setInterval(this._updateQueues.bind(this), QueueView.UpdateInterval);
        else if (!wasHidden && isHidden) {
            clearInterval(this.updateTimer);
            this.updateTimer = null;
        }
    }
};
