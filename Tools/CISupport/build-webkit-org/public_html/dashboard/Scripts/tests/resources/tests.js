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

module("BuildBotQueueView", {
    setup: function() {
        this.commitStore = new MockCommitStore();
        this.commitStore.commitsByBranch['someOtherBranch'] = [];
        this.queue = new MockBuildbotQueue();
        this.mainBranch = {
            name: "main",
            repository: {
                name: "openSource",
                commitStore: this.commitStore,
                isGit: true,
            }
        };
        this.queue.branches = [this.mainBranch];
        this.view = new MockBuildbotQueueView([this.queue]);
        this.view._latestProductiveIteration = function(queue)
        {
            var iteration = {
                revision: { "openSource": "33021@main" },
            };
            return iteration;
        }
    }
});

var settings = new Settings;
test("_appendPendingRevisionCount", function()
{
    this.view._appendPendingRevisionCount(this.queue, this.view._latestProductiveIteration);
    var revisionsBehind = this.view.element.getElementsByClassName("message")[0].innerHTML.match(/.*(\d+) commit(|s) behind/)[1];
    strictEqual(revisionsBehind, "1", "assert commits behind");
});

test("_popoverLinesForCommitRange", function()
{
    var lines = this.view._popoverLinesForCommitRange(this.commitStore, this.mainBranch, [
        {identifier: '26210@main'},
        {identifier: '26208@main'},
    ]);
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
            commitStore: this.commitStore,
            isSVN: false,
        }
    };
    this.queue.branches = [this.someOtherBranch];
    var element = document.createElement("div");
    var popover = new Dashboard.Popover();
    this.view._presentPopoverForPendingCommits(this.view._latestProductiveIteration, element, popover, this.queue);
    var nodeList = popover._element.getElementsByClassName("pending-commit");
    strictEqual(nodeList.length, 0, "has 0 pending commits");
});

test("_revisionContentWithPopoverForIteration", function()
{
    var finished = false;
    var iteration = new BuildbotIteration(this.queue, 1, finished);
    iteration.revision = { "openSource": "33018@main" };
    var previousIteration = null;
    var content = this.view._revisionContentWithPopoverForIteration(iteration, previousIteration, this.mainBranch);
    strictEqual(content.innerHTML, "33018@main", "should have correct revision number.");
    strictEqual(content.classList.contains("revision-number"), true, "should have class 'revision-number'.");
});

test("_revisionContentWithPopoverForIteration has previousIteration", function()
{
    var finished = false;
    var iteration = new BuildbotIteration(this.queue, 2, finished);
    iteration.revision = { "openSource": "33022@main" };
    var previousIteration = new BuildbotIteration(this.queue, 1, finished);
    previousIteration.revision = { "openSource": "33018@main" };
    var content = this.view._revisionContentWithPopoverForIteration(iteration, previousIteration, this.mainBranch);
    strictEqual(content.innerHTML, "33022@main", "should have correct revision number.");
    strictEqual(content.classList.contains("revision-number"), true, "should have class 'revision-number'.");
    var element = document.createElement("div");
    var popover = new Dashboard.Popover();
});

test("_formatRevisionForDisplay Git", function()
{
    var repository = this.mainBranch.repository;
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
        Dashboard.Repository.OpenSource.commitStore = new MockCommitStore();this.queue = new MockBuildbotQueue();
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
