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

Trac = function(baseURL, options)
{
    BaseObject.call(this);

    console.assert(baseURL);

    this.baseURL = baseURL;
    this._needsAuthentication = (typeof options === "object") && options[Trac.NeedsAuthentication] === true;

    this.recordedCommits = []; // Will be sorted in ascending order.

    this.update();
    this.updateTimer = setInterval(this.update.bind(this), Trac.UpdateInterval);
};

BaseObject.addConstructorFunctions(Trac);

Trac.NeedsAuthentication = "needsAuthentication";
Trac.UpdateInterval = 45000; // 45 seconds

Trac.Event = {
    NewCommitsRecorded: "new-commits-recorded"
};

Trac.prototype = {
    constructor: Trac,
    __proto__: BaseObject.prototype,

    get latestRecordedRevisionNumber()
    {
        if (!this.recordedCommits.length)
            return undefined;
        return this.recordedCommits[this.recordedCommits.length - 1].revisionNumber;
    },

    revisionURL: function(revision)
    {
        return this.baseURL + "changeset/" + encodeURIComponent(revision);
    },

    _xmlTimelineURL: function()
    {
        return this.baseURL + "timeline?changeset=on&max=50&format=rss";
    },

    _convertCommitInfoElementToObject: function(doc, commitElement)
    {
        var link = doc.evaluate("./link", commitElement, null, XPathResult.STRING_TYPE).stringValue;
        var revisionNumber = parseInt(/\d+$/.exec(link))

        function tracNSResolver(prefix)
        {
            if (prefix == "dc")
                return "http://purl.org/dc/elements/1.1/";
            return null;
        }

        var author = doc.evaluate("./author|dc:creator", commitElement, tracNSResolver, XPathResult.STRING_TYPE).stringValue;
        var date = doc.evaluate("./pubDate", commitElement, null, XPathResult.STRING_TYPE).stringValue;
        date = new Date(Date.parse(date));
        var description = doc.evaluate("./description", commitElement, null, XPathResult.STRING_TYPE).stringValue;

        // The feed contains a <title>, but it's not parsed as well as what we are getting from description.
        var parsedDescription = document.createElement("div");
        parsedDescription.innerHTML = description;
        var title = document.createElement("div");
        var node = parsedDescription.firstChild.firstChild;
        while (node && node.tagName != "BR") {
            title.appendChild(node.cloneNode(true));
            node = node.nextSibling;
        }

        // For some reason, trac titles start with a newline. Delete it.
        if (title.firstChild && title.firstChild.nodeType == Node.TEXT_NODE && title.firstChild.textContent.length > 0 && title.firstChild.textContent[0] == "\n")
            title.firstChild.textContent = title.firstChild.textContent.substring(1);

        return {
            revisionNumber: revisionNumber,
            link: link,
            title: title,
            author: author,
            date: date,
            description: description
        };
    },

    update: function()
    {
        loadXML(this._xmlTimelineURL(), function(dataDocument) {
            var latestKnownRevision = 0;
            if (this.recordedCommits.length)
                latestKnownRevision = this.recordedCommits[this.recordedCommits.length - 1].revisionNumber;

            var newCommits = [];

            var commitInfoElements = dataDocument.evaluate("/rss/channel/item", dataDocument, null, XPathResult.ORDERED_NODE_ITERATOR_TYPE);
            var commitInfoElement = undefined;
            while (commitInfoElement = commitInfoElements.iterateNext()) {
                var commit = this._convertCommitInfoElementToObject(dataDocument, commitInfoElement);
                if (commit.revisionNumber == latestKnownRevision)
                    break;
                newCommits.push(commit);
            }
            
            if (!newCommits.length)
                return;

            this.recordedCommits = this.recordedCommits.concat(newCommits.reverse());

            this.dispatchEventToListeners(Trac.Event.NewCommitsRecorded, {newCommits: newCommits});
        }.bind(this), this._needsAuthentication ? { withCredentials: true } : {});
    }
};
