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

Settings = function()
{
    BaseObject.call(this);

    this.changeCallbacks = {};
};

BaseObject.addConstructorFunctions(Settings);

Settings.prototype = {
    constructor: Settings,
    __proto__: BaseObject.prototype,

    setObject: function(key, value)
    {
        localStorage.setItem(key, JSON.stringify(value));
    },

    getObject: function(key)
    {
        return JSON.parse(localStorage.getItem(key));
    },

    available: function()
    {
        try {
            localStorage.setItem("testLocalStorage", "test");
            if (localStorage.getItem("testLocalStorage") != "test")
                return false;
            localStorage.removeItem("testLocalStorage");
        } catch(e) {
            console.log("Couldn't use localStorage, settings won't work: " + e);
            return false;
        }

        return true;
    },

    addSettingListener: function(key, callback)
    {
        if (!this.changeCallbacks[key])
            this.changeCallbacks[key] = [];
        this.changeCallbacks[key].push(callback);
    },

    fireSettingListener: function(key)
    {
        var callbacks = this.changeCallbacks[key];
        if (!callbacks)
            return;
        for (var i = 0; i < callbacks.length; ++i)
            callbacks[i]();
    },

    toggleSettingsDisplay: function()
    {
        document.body.classList.toggle("settings-visible");
    },

    toggleHiddenPlatform: function(platform)
    {
        var hiddenPlatforms = this.getObject("hiddenPlatforms");
        if (!hiddenPlatforms)
            hiddenPlatforms = [];

        var hiddenPlatformIndex = hiddenPlatforms.indexOf(platform);
        if (hiddenPlatformIndex > -1)
            hiddenPlatforms.splice(hiddenPlatformIndex, 1);
        else
            hiddenPlatforms.push(platform);

        this.setObject("hiddenPlatforms", hiddenPlatforms);
        this.fireSettingListener("hiddenPlatforms");
    },

    clearHiddenPlatforms: function()
    {
        this.setObject("hiddenPlatforms", []);
        this.fireSettingListener("hiddenPlatforms");
    },
};
