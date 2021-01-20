/*
 * Copyright (C) 2016 Apple, Inc. All rights reserved.
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

if (window.testRunner) {
    window.testRunner.dumpAsText();
    window.testRunner.waitUntilDone();
}

QUnit.done = function(details) {
    if (window.testRunner) {
        var element = document.getElementById("qunit-testresult");
        element.parentNode.removeChild(element);
        element = document.getElementById("qunit-userAgent");
        element.parentNode.removeChild(element);
        window.testRunner.notifyDone();
    }
};

module("Trac", {
    setup: function() {
        this.trac = new MockTrac();
        this.tracWithIdentifier = new MockTrac("webkit");
    }
});

test("_loaded", function()
{
    this.trac.recordedCommits = MockTrac.EXAMPLE_TRAC_COMMITS;
    var client = new XMLHttpRequest();
    client.open("GET", "resources/test-fixture-trac-rss.xml", false);
    client.onload = function () {
        this.trac._loaded(client.responseXML);
    }.bind(this);
    client.send();
    var commits = this.trac.recordedCommits;
    strictEqual(commits.length, 8, "should have 8 commits");
    for (var i = 1; i < commits.length; i++) {
        var firstRevision = commits[i - 1].revisionNumber;
        var secondRevision = commits[i].revisionNumber;
        strictEqual(secondRevision - firstRevision, 1, "commits should be in order " + firstRevision + ", " + secondRevision);
    }
});

test("parse gitBranches", function()
{
    var client = new XMLHttpRequest();
    client.open("GET", "resources/test-fixture-git-trac-rss.xml", false);
    client.onload = function () {
        this.trac._loaded(client.responseXML);
    }.bind(this);
    client.send();
    var commits = this.trac.recordedCommits;
    strictEqual(commits.length, 3, "should have 3 commits");
    strictEqual(commits[0].branches.length, 0, "should have no branches");
    strictEqual(commits[1].branches.length, 1, "should have one branch");
    strictEqual(commits[1].branches.includes("master"), true, "should contain branch master");
    strictEqual(commits[2].branches.length, 2, "should have two branches");
    strictEqual(commits[2].branches.includes("master"), true, "should contain branch master");
    strictEqual(commits[2].branches.includes("someOtherBranch"), true, "should contain branch someOtherBranch");
});

test("_parseRevisionFromURL", function()
{
    strictEqual(this.trac._parseRevisionFromURL("https://trac.webkit.org/changeset/190497"), "190497", "Subversion");
    strictEqual(this.trac._parseRevisionFromURL("http://trac.foobar.com/repository/changeset/75388/project"), "75388", "Subversion with suffix");
    strictEqual(this.trac._parseRevisionFromURL("https://git.foobar.com/trac/Whatever.git/changeset/0e498db5d8e5b5a342631"), "0e498db5d8e5b5a342631", "Git");
});

test("nextRevision", function()
{
    this.trac.recordedCommits = MockTrac.EXAMPLE_TRAC_COMMITS;
    this.trac.recordedCommitIndicesByRevisionNumber = MockTrac.recordedCommitIndicesByRevisionNumber;
    strictEqual(this.trac.nextRevision("trunk", "33020"), "33022", "nextRevision same branch");
    strictEqual(this.trac.nextRevision("trunk", "33019"), "33020", "nextRevision different branch");
});

test("indexOfRevision", function()
{
    this.trac.recordedCommits = MockTrac.EXAMPLE_TRAC_COMMITS;
    this.trac.recordedCommitIndicesByRevisionNumber = MockTrac.recordedCommitIndicesByRevisionNumber;
    strictEqual(this.trac.indexOfRevision("33020"), 2, "indexOfRevision");
});

test("commitsOnBranchLaterThanRevision", function()
{
    this.trac.recordedCommits = MockTrac.EXAMPLE_TRAC_COMMITS;
    this.trac.recordedCommitIndicesByRevisionNumber = MockTrac.recordedCommitIndicesByRevisionNumber;
    var commits = this.trac.commitsOnBranchLaterThanRevision("trunk", "33020");
    equal(commits.length, 1, "greater than 33020");
});

test("commitsOnBranchLaterThanRevision no commits", function()
{
    this.trac.recordedCommits = MockTrac.EXAMPLE_TRAC_COMMITS;
    this.trac.recordedCommitIndicesByRevisionNumber = MockTrac.recordedCommitIndicesByRevisionNumber;
    var commits = this.trac.commitsOnBranchLaterThanRevision("someOtherBranch", "33021");
    equal(commits.length, 0, "greater than 33021");
});

test("commitsOnBranchInRevisionRange", function()
{
    this.trac.recordedCommits = MockTrac.EXAMPLE_TRAC_COMMITS;
    this.trac.recordedCommitIndicesByRevisionNumber = MockTrac.recordedCommitIndicesByRevisionNumber;
    var commits = this.trac.commitsOnBranchInRevisionRange("trunk", "33020", "33022");
    equal(commits.length, 2, "in range 33020, 33022");
});

test("revisionURL", function()
{
    strictEqual(this.trac.revisionURL("33020"), "https://trac.webkit.org/changeset/33020", "changeset URL matches for 33020");
    strictEqual(this.trac.revisionURL("0e498db5d8e5b5a342631"), "https://trac.webkit.org/changeset/0e498db5d8e5b5a342631", "changeset URL matches for 0e498db5d8e5b5a342631");
});

test("revisionURL with Trac Identifier", function()
{
    strictEqual(this.tracWithIdentifier.revisionURL("33020"), "https://trac.webkit.org/changeset/33020/webkit", "changeset URL matches for 33020");
    strictEqual(this.tracWithIdentifier.revisionURL("0e498db5d8e5b5a342631"), "https://trac.webkit.org/changeset/0e498db5d8e5b5a342631/webkit", "changeset URL matches for 0e498db5d8e5b5a342631");
});

test("_xmlTimelineURL", function()
{
    var before = new Date("1/1/2017");
    var after = new Date("1/2/2017");

    strictEqual(this.trac._xmlTimelineURL(before, before), "https://trac.webkit.org/timeline?changeset=on&format=rss&max=0&from=2017-01-01&daysback=0");
    strictEqual(this.trac._xmlTimelineURL(before, after), "https://trac.webkit.org/timeline?changeset=on&format=rss&max=0&from=2017-01-02&daysback=1");
});

test("_xmlTimelineURL with Trac Identifier", function()
{
    var before = new Date("1/1/2017");
    var after = new Date("1/2/2017");

    strictEqual(this.tracWithIdentifier._xmlTimelineURL(before, before), "https://trac.webkit.org/timeline?repo-webkit=on&format=rss&max=0&from=2017-01-01&daysback=0");
    strictEqual(this.tracWithIdentifier._xmlTimelineURL(before, after), "https://trac.webkit.org/timeline?repo-webkit=on&format=rss&max=0&from=2017-01-02&daysback=1");
});

module("BuildBotQueueView", {
    setup: function() {
        this.trac = new MockTrac();
        this.trac.recordedCommits = MockTrac.EXAMPLE_TRAC_COMMITS;
        this.trac.recordedCommitIndicesByRevisionNumber = MockTrac.recordedCommitIndicesByRevisionNumber;
        this.queue = new MockBuildbotQueue();
        this.trunkBranch = {
            name: "trunk",
            repository: {
                name: "openSource",
                trac: this.trac,
                isSVN: true,
            }
        };
        this.queue.branches = [this.trunkBranch];
        this.view = new MockBuildbotQueueView([this.queue]);
        this.view._latestProductiveIteration = function(queue)
        {
            var iteration = {
                revision: { "openSource": "33021" },
            };
            return iteration;
        }
    }
});

var settings = new Settings;
test("_appendPendingRevisionCount", function()
{
    this.view._appendPendingRevisionCount(this.queue, this.view._latestProductiveIteration);
    var revisionsBehind = this.view.element.getElementsByClassName("message")[0].innerHTML.match(/.*(\d+) revision(|s) behind/)[1];
    strictEqual(revisionsBehind, "1", "assert revisions behind");
});

test("_popoverLinesForCommitRange", function()
{
    var lines = this.view._popoverLinesForCommitRange(this.trac, this.trunkBranch, "33018", "33020");
    strictEqual(lines.length, 2, "has 2 lines");
});

test("_presentPopoverForPendingCommits", function()
{
    var element = document.createElement("div");
    var popover = new Dashboard.Popover();
    this.view._presentPopoverForPendingCommits(this.view._latestProductiveIteration, element, popover, this.queue);
    var nodeList = popover._element.getElementsByClassName("pending-commit");
    strictEqual(nodeList.length, 1, "has 1 pending commit");
});

test("_presentPopoverForPendingCommits no pending commits", function()
{
    this.someOtherBranch = {
        name: "someOtherBranch",
        repository: {
            name: "openSource",
            trac: this.trac,
            isSVN: true,
        }
    };
    this.queue.branches = [this.someOtherBranch];
    var element = document.createElement("div");
    var popover = new Dashboard.Popover();
    this.view._presentPopoverForPendingCommits(this.view._latestProductiveIteration, element, popover, this.queue);
    var nodeList = popover._element.getElementsByClassName("pending-commit");
    strictEqual(nodeList.length, 0, "has 0 pending commits");
});

test("_presentPopoverForRevisionRange", function()
{
    var element = document.createElement("div");
    var popover = new Dashboard.Popover();
    var context = {
        trac: this.trac,
        branch: this.trunkBranch,
        firstRevision: "33018",
        lastRevision: "33020"
    };
    this.view._presentPopoverForRevisionRange(element, popover, context);
    var nodeList = popover._element.getElementsByClassName("pending-commit");
    strictEqual(nodeList.length, 2, "has 2 commits");
});

test("_presentPopoverForRevisionRange no commits", function()
{
    var element = document.createElement("div");
    var popover = new Dashboard.Popover();
    var context = {
        trac: this.trac,
        branch: this.trunkBranch,
        firstRevision: "33020",
        lastRevision: "33018"
    };
    this.view._presentPopoverForRevisionRange(element, popover, context);
    var nodeList = popover._element.getElementsByClassName("pending-commit");
    strictEqual(nodeList.length, 0, "has 0 commits");
});

test("_revisionContentWithPopoverForIteration", function()
{
    var finished = false;
    var iteration = new BuildbotIteration(this.queue, 1, finished);
    iteration.revision = { "openSource": "33018" };
    var previousIteration = null;
    var content = this.view._revisionContentWithPopoverForIteration(iteration, previousIteration, this.trunkBranch);
    strictEqual(content.innerHTML, "r33018", "should have correct revision number.");
    strictEqual(content.classList.contains("revision-number"), true, "should have class 'revision-number'.");
    strictEqual(content.classList.contains("popover-tracking"), false, "should not have class 'popover-tracking'.");
});

test("_revisionContentWithPopoverForIteration has previousIteration", function()
{
    var finished = false;
    var iteration = new BuildbotIteration(this.queue, 2, finished);
    iteration.revision = { "openSource": "33022" };
    var previousIteration = new BuildbotIteration(this.queue, 1, finished);
    previousIteration.revision = { "openSource": "33018" };
    var content = this.view._revisionContentWithPopoverForIteration(iteration, previousIteration, this.trunkBranch);
    strictEqual(content.innerHTML, "r33022", "should have correct revision number.");
    strictEqual(content.classList.contains("revision-number"), true, "should have class 'revision-number'.");
    strictEqual(content.classList.contains("popover-tracking"), true, "should have class 'popover-tracking'.");
    var element = document.createElement("div");
    var popover = new Dashboard.Popover();
    this.view._presentPopoverForRevisionRange(element, popover, content.popoverTracker._context);
    var nodeList = popover._element.getElementsByClassName("pending-commit");
    strictEqual(nodeList.length, 2, "has 2 commits");
});

test("_formatRevisionForDisplay Subversion", function()
{
    var repository = this.trunkBranch.repository;
    repository.isSVN = true;
    repository.isGit = false;
    strictEqual(this.view._formatRevisionForDisplay("33018", repository), "r33018", "Should be r33018")
});

test("_formatRevisionForDisplay Git", function()
{
    var repository = this.trunkBranch.repository;
    repository.isSVN = false;
    repository.isGit = true;
    strictEqual(this.view._formatRevisionForDisplay("0e498db5d8e5b5a342631", repository), "0e498db", "Should be 0e498db");
});

test("_popoverContentForJavaScriptCoreTestRegressions load failure UI test", function()
{
    var finished = false;
    var iteration = new BuildbotIteration(this.queue, 1, finished);
    iteration.javaScriptCoreTestResults = new MockBuildbotTestResults();

    var view = new BuildbotQueueView();
    var content = view._popoverContentForJavaScriptCoreTestRegressions(iteration);

    var numChildrenInEmptyPopoverContent = 2;
    strictEqual(content.childNodes.length, 1 + numChildrenInEmptyPopoverContent);
    strictEqual(content.childNodes[numChildrenInEmptyPopoverContent].className, "loading-failure", "Popover for loading failure must have the appropriate class");
    strictEqual(content.childNodes[numChildrenInEmptyPopoverContent].textContent, "Test results couldn\u2019t be loaded", "Popover for loading failure must use the correct text");
});

test("_presentPopoverForJavaScriptCoreTestRegressions including loading", function()
{
    var finished = false;
    var element = document.createElement("div");
    var popover = new Dashboard.Popover();
    var iteration = new BuildbotIteration(this.queue, 1, finished);
    iteration.javaScriptCoreTestResults = new MockBuildbotTestResults();

    var view = new BuildbotQueueView();
    view._presentPopoverForJavaScriptCoreTestRegressions("jscore-test", element, popover, iteration);

    JSON.load("resources/test-jsc-results.json", function(data)
    {
        var testRegressions = data.stressTestFailures;
        var numChildrenInEmptyPopoverContent = 2;
        strictEqual(popover._content.childNodes.length - numChildrenInEmptyPopoverContent,
            testRegressions.length,
            "Number of failures in popover must be equal to number of failed tests");

        for (var i = 0; i < testRegressions.length; i++)
        {
            strictEqual(popover._content.childNodes[i+numChildrenInEmptyPopoverContent].childNodes[0].textContent,
                testRegressions[i],
                "Names of failures must match"
            );
        }
    });
});

test("_createLoadingIndicator", function()
{
    var finished = false;
    var iteration = new BuildbotIteration(this.queue, 1, finished);

    var view = new BuildbotQueueView();
    var heading = "Fair is foul, and foul is fair";
    var content = view._createLoadingIndicator(iteration, heading);

    var numChildrenInEmptyPopoverContent = 2;
    strictEqual(content.childNodes.length, 1 + numChildrenInEmptyPopoverContent);
    strictEqual(content.childNodes[numChildrenInEmptyPopoverContent].className, "loading-indicator", "Popover for loading indicator must have the appropriate class");
    strictEqual(content.childNodes[numChildrenInEmptyPopoverContent].textContent, "Loading\u2026", "Popover for loading failure must use the correct text");
});

test("_presentPopoverForJavaScriptCoreTestRegressions already loaded", function()
{
    var finished = false;
    var element = document.createElement("div");
    var popover = new Dashboard.Popover();
    var iteration = new BuildbotIteration(this.queue, 1, finished);
    iteration.javaScriptCoreTestResults = {"regressions": ["uno", "dos", "tres"]};

    var view = new BuildbotQueueView();
    view._presentPopoverForJavaScriptCoreTestRegressions("jscore-test", element, popover, iteration);

    var numChildrenInEmptyPopoverContent = 2;
    strictEqual(popover._content.childNodes.length - numChildrenInEmptyPopoverContent,
        iteration.javaScriptCoreTestResults.regressions.length,
        "Number of failures in popover must be equal to number of failed tests");

    for (var i = 0; i < iteration.javaScriptCoreTestResults.regressions.length; i++)
    {
        strictEqual(popover._content.childNodes[i+numChildrenInEmptyPopoverContent].childNodes[0].textContent,
            iteration.javaScriptCoreTestResults.regressions[i],
            "Names of failures must match"
        );
    }
});

module("BuildBotQueue", {
    setup: function() {
        Dashboard.Repository.OpenSource.trac = new MockTrac();
        Dashboard.Repository.OpenSource.trac.recordedCommits = MockTrac.EXAMPLE_TRAC_COMMITS;
        Dashboard.Repository.OpenSource.trac.recordedCommitIndicesByRevisionNumber = MockTrac.recordedCommitIndicesByRevisionNumber;
        this.queue = new MockBuildbotQueue();
        this.queue.branches = [{
            name: "trunk",
            repository: Dashboard.Repository.Opensource
        }];
    }
});

test("compareIterations by revisions", function()
{
    var finished = false;
    var iteration1 = new BuildbotIteration(this.queue, 1, finished);
    var iteration2 = new BuildbotIteration(this.queue, 2, finished);
    iteration1.revision = { "openSource": "33018" };
    iteration2.revision = { "openSource": "33019" };
    iteration1.loaded = true;
    iteration2.loaded = true;
    ok(this.queue.compareIterations(iteration2, iteration1) < 0, "compareIterations: less than");
    ok(this.queue.compareIterations(iteration1, iteration2) > 0, "compareIterations: greater than");
    strictEqual(this.queue.compareIterations(iteration2, iteration2), 0, "compareIterations: equal");
});

test("compareIterations by loaded (one revision missing)", function()
{
    var finished = false;
    var iteration1 = new BuildbotIteration(this.queue, 1, finished);
    var iteration2 = new BuildbotIteration(this.queue, 2, finished);
    iteration1.revision = {};
    iteration2.revision = { "openSource": "33019" };
    iteration1.loaded = false;
    iteration2.loaded = true;
    ok(this.queue.compareIterations(iteration1, iteration2) > 0, "compareIterations: greater than");
    ok(this.queue.compareIterations(iteration2, iteration1) < 0, "compareIterations: less than");
});

test("compareIterations by loaded (same revision)", function()
{
    var finished = false;
    var iteration1 = new BuildbotIteration(this.queue, 1, finished);
    var iteration2 = new BuildbotIteration(this.queue, 2, finished);
    iteration1.revision = { "openSource": "33019" };
    iteration2.revision = { "openSource": "33019" };
    iteration1.loaded = false;
    iteration2.loaded = true;
    ok(this.queue.compareIterations(iteration1, iteration2) > 0, "compareIterations: greater than");
    ok(this.queue.compareIterations(iteration2, iteration1) < 0, "compareIterations: less than");
});

test("compareIterations by id (revisions not specified)", function()
{
    var finished = false;
    var iteration1 = new BuildbotIteration(this.queue, 1, finished);
    var iteration2 = new BuildbotIteration(this.queue, 2, finished);
    iteration1.revision = {};
    iteration2.revision = {};
    iteration1.loaded = false;
    iteration2.loaded = false;
    ok(this.queue.compareIterations(iteration2, iteration1) < 0, "compareIterations: less than");
    ok(this.queue.compareIterations(iteration1, iteration2) > 0, "compareIterations: greater than");
    strictEqual(this.queue.compareIterations(iteration2, iteration2), 0, "compareIterations: equal");
});

test("compareIterations by id (same revision)", function()
{
    var finished = false;
    var iteration1 = new BuildbotIteration(this.queue, 1, finished);
    var iteration2 = new BuildbotIteration(this.queue, 2, finished);
    iteration1.revision = { "openSource": "33019" };
    iteration2.revision = { "openSource": "33019" };
    iteration1.loaded = false;
    iteration2.loaded = false;
    ok(this.queue.compareIterations(iteration2, iteration1) < 0, "compareIterations: less than");
    ok(this.queue.compareIterations(iteration1, iteration2) > 0, "compareIterations: greater than");
    strictEqual(this.queue.compareIterations(iteration2, iteration2), 0, "compareIterations: equal");
});

test("compareIterationsByRevisions", function()
{
    var finished = false;
    var iteration1 = new BuildbotIteration(this.queue, 1, finished);
    var iteration2 = new BuildbotIteration(this.queue, 2, finished);
    iteration1.revision = { "openSource": "33018" };
    iteration2.revision = { "openSource": "33019" };
    iteration1.loaded = true;
    iteration2.loaded = false;
    ok(this.queue.compareIterationsByRevisions(iteration2, iteration1) < 0, "compareIterationsByRevisions: less than");
    ok(this.queue.compareIterationsByRevisions(iteration1, iteration2) > 0, "compareIterationsByRevisions: greater than");
    strictEqual(this.queue.compareIterationsByRevisions(iteration2, iteration2), 0, "compareIterationsByRevisions: equal");
});
