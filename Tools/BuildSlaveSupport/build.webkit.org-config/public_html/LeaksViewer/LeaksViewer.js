/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

var LeaksViewer = {
    loaded: function() {
        this._loadingIndicator = document.getElementById("loading-indicator");

        this._profileView = new WebInspector.CPUProfileView({});
        document.getElementById("main-panels").appendChild(this._profileView.element);
        this._profileView.show();

        // From WebInspector.Panel.prototype.show
        var statusBarItems = this._profileView.statusBarItems;
        if (statusBarItems) {
            this._statusBarItemContainer = document.createElement("div");
            for (var i = 0; i < statusBarItems.length; ++i)
                this._statusBarItemContainer.appendChild(statusBarItems[i]);
            document.getElementById("main-status-bar").appendChild(this._statusBarItemContainer);
        }

        var url;
        var match = /url=([^&]+)/.exec(location.search);
        if (match)
            url = match[1];

        if (url)
            this._loadLeaksFromURL(url)
        else
            this._displayURLPrompt();
    },

    get loading() {
        return this._isLoading;
    },

    set loading(x) {
        if (this._isLoading === x)
            return;
        this._isLoading = x;
        this._loadingStatusChanged();
    },

    get url() {
        return this._url;
    },

    set url(x) {
        if (this._url === x)
            return;

        this._url = x;
        this._updateTitle();
    },

    urlPromptButtonClicked: function(e) {
        this._loadLeaksFromURL(document.getElementById("url").value);
        document.getElementById("url-prompt-container").addStyleClass("hidden");
    },

    _displayURLPrompt: function() {
        document.getElementById("url-prompt-container").removeStyleClass("hidden");
        document.getElementById("url").focus();
    },

    _loadLeaksFromURL: function(url) {
        this.url = url;
        this.loading = true;

        if (/\.txt$/.test(this.url)) {
            // We're loading a single leaks file.

            var self = this;
            getResource(url, function(xhr) {
                var worker = new Worker("Worker.js");
                worker.onmessage = function(e) {
                    ProfilerAgent.profileReady(e.data);
                    self.loading = false;
                };
                worker.postMessage(xhr.responseText);
            });
        } else {
            function mergeProfiles(a, b) {
                a.selfTime += b.selfTime;
                a.totalTime += b.totalTime;

                b.children.forEach(function(child) {
                    var aChild = a.childrenByName[child.functionName];
                    if (aChild) {
                        mergeProfiles(aChild, child);
                        return;
                    }

                    a.children.push(child);
                    a.childrenByName[child.functionName] = child;
                });
            }
            // Assume we're loading a results directory. Try to find all the leaks files in it.
            var self = this;
            getResource(url, function(xhr) {
                var root = document.createElement("html");
                root.innerHTML = xhr.responseText;

                // Strip off everything after the last /.
                var baseURL = url.substring(0, url.lastIndexOf("/") + 1);
                var urls = Array.prototype.map.call(root.querySelectorAll("tr.file > td > a[href$='-leaks.txt']"), function(link) { return baseURL + link.getAttribute("href"); });

                var pendingProfilesCount = urls.length;
                var profile;
                urls.forEach(function(url) {
                    getResource(url, function(xhr) {
                        var worker = new Worker("Worker.js");
                        worker.onmessage = function(e) {
                            if (profile)
                                mergeProfiles(profile, e.data);
                            else
                                profile = e.data;
                            if (--pendingProfilesCount)
                                return;
                            ProfilerAgent.profileReady(profile);
                            self.loading = false;
                        };
                        worker.postMessage(xhr.responseText);
                    });
                });
            });
        }
    },

    _loadingStatusChanged: function() {
        this._setLoadingIndicatorHidden(!this.loading);
        this._updateTitle();
    },

    _setLoadingIndicatorHidden: function(hidden) {
        if (hidden)
            this._loadingIndicator.addStyleClass("hidden");
        else
            this._loadingIndicator.removeStyleClass("hidden");
    },

    _updateTitle: function() {
        var title = "Leaks Viewer \u2014 ";
        if (this.loading)
            title += "(Loading\u2026) ";
        title += this.url;
        document.title = title;
    },
};

function getResource(url, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
        // Allow a status of 0 for easier testing with local files.
        if (this.readyState == 4 && (!this.status || this.status == 200))
            callback(this);
    };
    xhr.open("GET", url);
    xhr.send();
}

addEventListener("load", LeaksViewer.loaded.bind(LeaksViewer));
