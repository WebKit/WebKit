/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
    if (typeof options === "object") {
        this._needsAuthentication = options[Trac.NeedsAuthentication] === true;
        // We expect the projectIdentifier option iff the target Trac instance is hosting multiple repositories && Trac version > 1.0
        this._projectName = options[Trac.ProjectIdentifier];
    }

    this.recordedCommits = []; // Will be sorted in ascending order.
    this.recordedCommitIndicesByRevisionNumber = {};
};

BaseObject.addConstructorFunctions(Trac);

Trac.NO_MORE_REVISIONS = null;

Trac.NeedsAuthentication = "needsAuthentication";
Trac.ProjectIdentifier = "projectIdentifier";
Trac.UpdateInterval = 45000; // 45 seconds

Trac.Event = {
    CommitsUpdated: "commits-updated",
    Loaded: "loaded"
};

Trac.prototype = {
    constructor: Trac,
    __proto__: BaseObject.prototype,

    get oldestRecordedRevisionNumber()
    {
        if (!this.recordedCommits.length)
            return undefined;
        return this.recordedCommits[0].revisionNumber;
    },

    get latestRecordedRevisionNumber()
    {
        if (!this.recordedCommits.length)
            return undefined;
        return this.recordedCommits[this.recordedCommits.length - 1].revisionNumber;
    },

    _commitsOnBranch: function(branchName, beginPosition, endPosition)
    {
        beginPosition = beginPosition || 0;
        if (endPosition === undefined)
            endPosition = this.recordedCommits.length - 1;
        var commits = [];
        for (var i = beginPosition; i <= endPosition; ++i) {
            var commit = this.recordedCommits[i];
            if (!commit.containsBranchLocation || commit.branches.includes(branchName))
                commits.push(commit);
        }
        return commits;
    },

    commitsOnBranchLaterThanRevision: function(branchName, revision)
    {
        var indexToBeLaterThan = this.indexOfRevision(revision);
        console.assert(indexToBeLaterThan !== -1, revision + " is not in the list of recorded commits");
        if (indexToBeLaterThan === -1)
            return [];
        return this._commitsOnBranch(branchName, indexToBeLaterThan + 1);
    },

    commitsOnBranchInRevisionRange: function(branchName, firstRevision, lastRevision)
    {
        var indexOfFirstRevision = this.indexOfRevision(firstRevision);
        console.assert(indexOfFirstRevision !== -1, firstRevision + " is not in the list of recorded commits");
        if (indexOfFirstRevision === -1)
            return [];
        var indexOfLastRevision = this.indexOfRevision(lastRevision);
        console.assert(indexOfLastRevision !== -1, lastRevision + " is not in the list of recorded commits");
        if (indexOfLastRevision === -1)
            return [];
        return this._commitsOnBranch(branchName, indexOfFirstRevision, indexOfLastRevision);
    },

    revisionURL: function(revision)
    {
        var url = this.baseURL + "changeset/" + encodeURIComponent(revision);
        if (this._projectName)
            url += '/' + encodeURIComponent(this._projectName);
        return url;
    },

    _xmlTimelineURL: function(fromDate, toDate)
    {
        console.assert(fromDate <= toDate);

        var fromDay = new Date(fromDate.getFullYear(), fromDate.getMonth(), fromDate.getDate());
        var toDay = new Date(toDate.getFullYear(), toDate.getMonth(), toDate.getDate());
        var changesetParameter = this._projectName ? "repo-" + this._projectName : "changeset";

        return this.baseURL + "timeline?" +
            changesetParameter + "=on" +
            "&format=rss" +
            "&max=0" +
            "&from=" + encodeURIComponent(toDay.toISOString().slice(0, 10)) +
            "&daysback=" + encodeURIComponent((toDay - fromDay) / 1000 / 60 / 60 / 24);
    },

    _parseRevisionFromURL: function(url)
    {
        // There are multiple link formats for Trac that we support:
        // https://trac.webkit.org/changeset/190497
        // http://trac.foobar.com/repository/changeset/75388/project
        // https://git.foobar.com/trac/Whatever.git/changeset/0e498db5d8e5b5a342631
        return /changeset\/([a-f0-9]+).*$/.exec(url)[1];
    },

    _convertCommitInfoElementToObject: function(doc, commitElement)
    {
        var link = doc.evaluate("./link", commitElement, null, XPathResult.STRING_TYPE).stringValue;
        var revisionNumber = this._parseRevisionFromURL(link);

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

        var parsedDescription = document.createElement("div");
        parsedDescription.innerHTML = description;

        var location = "";
        if (parsedDescription.firstChild && parsedDescription.firstChild.className === "changes") {
            // We can extract branch information when trac.ini contains "changeset_show_files=location".
            location = doc.evaluate("//strong", parsedDescription.firstChild, null, XPathResult.STRING_TYPE).stringValue;
            parsedDescription.removeChild(parsedDescription.firstChild);
        }

        // The feed contains a <title>, but it's not parsed as well as what we are getting from description.
        var title = document.createElement("div");
        var node = parsedDescription.firstChild ? parsedDescription.firstChild.firstChild : null;
        while (node && node.tagName != "BR") {
            title.appendChild(node.cloneNode(true));
            node = node.nextSibling;
        }

        // For some reason, trac titles start with a newline. Delete it.
        if (title.firstChild && title.firstChild.nodeType == Node.TEXT_NODE && title.firstChild.textContent.length > 0 && title.firstChild.textContent[0] == "\n")
            title.firstChild.textContent = title.firstChild.textContent.substring(1);

        // We have an overidden timeline.rss that adds git branches to the Trac timeline RSS output (rdar://problem/23853623).
        var gitBranches = doc.evaluate("./branches", commitElement, null, XPathResult.STRING_TYPE).stringValue;

        var result = {
            revisionNumber: revisionNumber,
            link: link,
            title: title,
            author: author,
            date: date,
            description: parsedDescription.innerHTML,
            containsBranchLocation: location !== "",
            branches: []
        };

        if (result.containsBranchLocation && !gitBranches) {
            console.assert(location[location.length - 1] !== "/");
            location = location += "/";
            if (location.startsWith("tags/"))
                result.tag = location.substr(5, location.indexOf("/", 5) - 5);
            else if (location.startsWith("branches/"))
                result.branches.push(location.substr(9, location.indexOf("/", 9) - 9));
            else if (location.startsWith("releases/"))
                result.release = location.substr(9, location.indexOf("/", 9) - 9);
            else if (location.startsWith("trunk/"))
                result.branches.push("trunk");
            else if (location.startsWith("submissions/"))
                ; // These changes are never relevant to the dashboard.
            else {
                // result.containsBranchLocation remains true, because this commit does
                // not match any explicitly specified branches.
                console.assert(false);
            }
        }

        if (gitBranches) {
            result.containsBranchLocation = true;
            result.branches = result.branches.concat(gitBranches.split(", "));
        }

        return result;
    },

    _loaded: function(dataDocument)
    {
        if (!dataDocument)
            return;

        var knownCommitsWereUpdated = false;
        var newCommits = [];

        var commitInfoElements = dataDocument.evaluate("/rss/channel/item", dataDocument, null, XPathResult.ORDERED_NODE_ITERATOR_TYPE);
        var commitInfoElement;
        while (commitInfoElement = commitInfoElements.iterateNext()) {
            var commit = this._convertCommitInfoElementToObject(dataDocument, commitInfoElement);
            var knownCommitIndex = this.recordedCommitIndicesByRevisionNumber[commit.revisionNumber];
            if (knownCommitIndex >= 0) {
                // Author could have changed, as commit queue replaces it after the fact.
                console.assert(this.recordedCommits[knownCommitIndex].revisionNumber === commit.revisionNumber);
                if (this.recordedCommits[knownCommitIndex].author != commit.author) {
                    this.recordedCommits[knownCommitIndex].author = commit.author;
                    knownCommitWasUpdated = true;
                }
            } else
                newCommits.push(commit);
        }

        if (newCommits.length) {
            this.recordedCommits = newCommits.concat(this.recordedCommits).sort(function(a, b) { return a.date - b.date; });
            this.recordedCommitIndicesByRevisionNumber = {};
            this.recordedCommits.forEach(function(curentValue, index) {
                this.recordedCommitIndicesByRevisionNumber[curentValue.revisionNumber] = index;
            }, this);
        }

        if (newCommits.length || knownCommitsWereUpdated)
            this.dispatchEventToListeners(Trac.Event.CommitsUpdated, null);
    },

    load: function(fromDate, toDate)
    {
        loadXML(this._xmlTimelineURL(fromDate, toDate), function(dataDocument) {
            this._loaded(dataDocument);
            this.dispatchEventToListeners(Trac.Event.Loaded, [fromDate, toDate]);
        }.bind(this), this._needsAuthentication ? { withCredentials: true } : {});
    },

    _update: function()
    {
        var fromDate = new Date(this._latestLoadedDate);
        var toDate = new Date();

        this._latestLoadedDate = toDate;

        loadXML(this._xmlTimelineURL(fromDate, toDate), this._loaded.bind(this), this._needsAuthentication ? { withCredentials: true } : {});
    },

    startPeriodicUpdates: function()
    {
        console.assert(!this._oldestHistoricalDate);

        var today = new Date();

        this._oldestHistoricalDate = today;
        this._latestLoadedDate = today;

        this._loadingHistoricalData = true;
        loadXML(this._xmlTimelineURL(today, today), function(dataDocument) {
            this._loadingHistoricalData = false;
            this._loaded(dataDocument);
        }.bind(this), this._needsAuthentication ? { withCredentials: true } : {});

        this.updateTimer = setInterval(this._update.bind(this), Trac.UpdateInterval);
    },

    loadMoreHistoricalData: function()
    {
        console.assert(this._oldestHistoricalDate);

        if (this._loadingHistoricalData)
            return;

        // Load one more day of historical data.
        var fromDate = new Date(this._oldestHistoricalDate);
        fromDate.setDate(fromDate.getDate() - 1);
        var toDate = new Date(fromDate);

        this._oldestHistoricalDate = fromDate;

        this._loadingHistoricalData = true;
        loadXML(this._xmlTimelineURL(fromDate, toDate), function(dataDocument) {
            this._loadingHistoricalData = false;
            this._loaded(dataDocument);
        }.bind(this), this._needsAuthentication ? { withCredentials: true } : {});
    },

    nextRevision: function(branchName, revision)
    {
        var commits = this.commitsOnBranchLaterThanRevision(branchName, revision);
        if (commits.length > 0)
            return commits[0].revisionNumber;
        return Trac.NO_MORE_REVISIONS;
    },

    indexOfRevision: function(revisionNumber)
    {
        var result = this.recordedCommitIndicesByRevisionNumber[revisionNumber];
        // FIXME: Update callers to handle undefined result.
        if (result === undefined)
            return -1;
        return result;
    },
};
