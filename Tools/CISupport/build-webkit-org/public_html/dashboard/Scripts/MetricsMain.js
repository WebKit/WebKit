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

var hasBubbles = typeof bubbleQueueServer != "undefined";

var analyzer = new Analyzer;

var allBuilderResultsPseudoQueue = { id: "allBuilderResultsPseudoQueue" };
var allResultsPseudoQueue = { id: "allResultsPseudoQueue" };

var categorizedQueuesByPlatformAndBuildType = {};
var platformsByFamily = {};

for (var i = 0; i < buildbots.length; ++i) {
    var buildbot = buildbots[i];
    for (var id in buildbot.queues) {
        var queue = buildbot.queues[id];
        var platform = categorizedQueuesByPlatformAndBuildType[queue.platform];
        if (!platform)
            platform = categorizedQueuesByPlatformAndBuildType[queue.platform] = {};
        if (!platform.builders)
            platform.builders = {};

        var categoryName;
        if (queue.builder)
            categoryName = "builders";
        else if (queue.tester)
            categoryName = queue.testCategory;
        else {
            console.assert("Unknown queue type.");
            continue;
        }

        category = platform[categoryName];
        if (!category)
            category = platform[categoryName] = {};

        var buildType = queue.debug ? "debug" : "release";

        buildQueues = category[buildType];
        if (!buildQueues)
            buildQueues = category[buildType] = [];

        buildQueues.push(queue);
    }
}

var testNames = {};
testNames[Buildbot.TestCategory.WebKit2] = "WK2 Tests";
testNames[Buildbot.TestCategory.WebKit1] = "WK1 Tests";


function updateHiddenPlatforms()
{
    var hiddenPlatformFamilies = settings.getObject("hiddenPlatformFamilies") || [];
    var platformRows = document.querySelectorAll("tr.platform");
    for (var i = 0; i < platformRows.length; ++i)
        platformRows[i].classList.remove("hidden");

    for (var i = 0; i < hiddenPlatformFamilies.length; ++i) {
        var platformFamily = hiddenPlatformFamilies[i];
        for (var j = 0; j < platformsByFamily[platformFamily].length; ++j) {
            var name = platformsByFamily[platformFamily][j];
            var platformRow = document.querySelector("tr.platform." + name);
            if (platformRow)
                platformRow.classList.add("hidden");
        }
    }
    settings.updateToggleButtons();
}


function initPlatformsByFamily()
{
    var platforms = Dashboard.sortedPlatforms;
    for (var i in platforms) {
        // Make sure the platform will be displayed on the page before considering its platform family.
        if (!categorizedQueuesByPlatformAndBuildType[platforms[i].name])
            continue;

        var platformFamily = settings.parsePlatformFamily(platforms[i].name);
        if (platformsByFamily[platformFamily])
            platformsByFamily[platformFamily].push(platforms[i].name)
        else
            platformsByFamily[platformFamily] = [platforms[i].name]
    }
}

function unhiddenQueues()
{
    var hiddenPlatformFamilies = settings.getObject("hiddenPlatformFamilies") || [];
    var result = [];
    for (var i = 0; i < buildbots.length; ++i) {
        var buildbot = buildbots[i];
        for (var id in buildbot.queues) {
            var queue = buildbot.queues[id];
            if (-1 === hiddenPlatformFamilies.indexOf(settings.parsePlatformFamily(queue.platform)))
                result.push(queue);
        }
    }

    return result;
}

function allQueues(category)
{
    var result = [];
    if (category["debug"])
        result = result.concat(category["debug"]);
    if (category["release"])
        result = result.concat(category["release"]);
    return result;
}

function buildAggregateTable()
{
    var table = document.createElement("table");
    table.classList.add("queue-grid");
    table.classList.add("aggregate-grid");

    var row = document.createElement("tr");
    row.classList.add("headers");

    header = document.createElement("th");
    header.textContent = "Builders";
    row.appendChild(header);

    header = document.createElement("th");
    header.textContent = "All queues";
    row.appendChild(header);

    table.appendChild(row);

    var row = document.createElement("tr");

    cell = document.createElement("td");

    var view = new MetricsView(analyzer, [allBuilderResultsPseudoQueue]);
    cell.appendChild(view.element);
    row.appendChild(cell);

    cell = document.createElement("td");

    var view = new MetricsView(analyzer, [allResultsPseudoQueue]);
    cell.appendChild(view.element);
    row.appendChild(cell);

    table.appendChild(row);

    document.body.appendChild(table);
}

function buildQueuesTable()
{
    var table = document.createElement("table");
    table.classList.add("queue-grid");
    table.id = "metrics-queues-table";

    var row = document.createElement("tr");
    row.classList.add("headers");

    var header = document.createElement("th");
    row.appendChild(header);

    header = document.createElement("th");
    header.textContent = "Builders";
    row.appendChild(header);

    for (var testerKey in Buildbot.TestCategory) {
        var header = document.createElement("th");
        header.textContent = testNames[Buildbot.TestCategory[testerKey]];
        row.appendChild(header);
    }

    table.appendChild(row);

    var platforms = Dashboard.sortedPlatforms;

    for (var i in platforms) {
        var platform = platforms[i];
        var platformQueues = categorizedQueuesByPlatformAndBuildType[platform.name];
        if (!platformQueues)
            continue;

        var row = document.createElement("tr");
        row.classList.add("platform");
        row.classList.add(platform.name);

        var cell = document.createElement("td");
        cell.classList.add("logo");

        var ringImage = document.createElement("img");
        ringImage.classList.add("ring");
        ringImage.title = platform.readableName;
        cell.appendChild(ringImage);

        var logoImage = document.createElement("img");
        logoImage.classList.add("logo");
        cell.appendChild(logoImage);

        row.appendChild(cell);

        cell = document.createElement("td");

        var view = new MetricsView(analyzer, allQueues(platformQueues.builders));
        cell.appendChild(view.element);
        row.appendChild(cell);

        for (var testerKey in Buildbot.TestCategory) {
            var cell = document.createElement("td");

            var testerProperty = Buildbot.TestCategory[testerKey];
            if (platformQueues[testerProperty]) {
                var view = new MetricsView(analyzer, allQueues(platformQueues[testerProperty]));
                cell.appendChild(view.element);
            }

            row.appendChild(cell);
        }

        table.appendChild(row);
    }

    document.body.appendChild(table);

    return table;
}

function buildBubbleQueuesTable()
{
    var table = document.createElement("table");
    table.classList.add("queue-grid");

    var row = document.createElement("tr");
    row.classList.add("headers");

    for (id in bubbleQueueServer.queues) {
        var header = document.createElement("th");
        header.textContent = bubbleQueueServer.queues[id].shortName;
        row.appendChild(header);
    }

    table.appendChild(row);

    row = document.createElement("tr");
    row.classList.add("platform");

    for (id in bubbleQueueServer.queues) {
        var cell = document.createElement("td");
        var view = new MetricsBubbleView(analyzer, bubbleQueueServer.queues[id]);
        cell.appendChild(view.element);
        row.appendChild(cell);
    }

    table.appendChild(row);

    document.body.appendChild(table);

    return table;
}

function documentReady()
{
    var rangePicker = document.createElement("span");
    rangePicker.id = "range-picker";
    rangePicker.size = 40;
    rangePicker.textContent = "Please select a date range";
    document.body.appendChild(rangePicker);
    $("#range-picker").dateRangePicker({
        startOfWeek: navigator.language === "en-us" ? "sunday" : "monday",
        endDate: new Date(),
        showShortcuts: false,
        getValue: function() { return this.textContent; },
        setValue: function(s) { this.textContent = s; }
    })
    .bind('datepicker-apply',function(event,obj)
    {
        // This message is dispatched even when range hasn't been selected (or only one endpoint was),
        // in which case the actual range hasn't changed, and there is nothing to do.
        if (isNaN(obj.date1) || isNaN(obj.date2))
            return;

        var endDate = new Date(new Date(obj.date2).setDate(obj.date2.getDate() + 1));

        // FIXME: Support performance bots.
        var queues = unhiddenQueues().filter(function(queue) { return queue.builder || queue.tester; });
        analyzer.analyze(queues, obj.date1, endDate);
    });

    var loadingIndicator = document.createElement("div");
    loadingIndicator.classList.add("metrics-loading-indicator");
    loadingIndicator.textContent = "Loading (first load takes several minutes)\u2026";
    document.body.appendChild(loadingIndicator);

    analyzer.addEventListener(Analyzer.Event.Starting, function() {
        loadingIndicator.style.visibility = "visible";
    }, this);
    analyzer.addEventListener(Analyzer.Event.Finished, function() {
        loadingIndicator.style.visibility = "hidden";
        loadingIndicator.textContent = "Loading\u2026"; // Remove the first time warning now.
    }, this);

    buildAggregateTable();
    if (hasBubbles) {
        var tablesDivider = document.createElement("div");
        tablesDivider.classList.add("tables-divider");
        document.body.appendChild(tablesDivider);
        buildBubbleQueuesTable();
    }

    var tablesDivider = document.createElement("div");
    tablesDivider.classList.add("tables-divider");
    document.body.appendChild(tablesDivider);

    var queuesTable = buildQueuesTable();
    tablesDivider.style.height = Math.max(tablesDivider.offsetHeight, window.innerHeight - tablesDivider.offsetTop) + "px";

    if (settings.available()) {
        var settingsButton = document.createElement("div");
        settingsButton.addEventListener("click", function () { settings.toggleSettingsDisplay(); });
        settingsButton.classList.add("settings");
        document.body.appendChild(settingsButton);

        var settingsWrapper = document.createElement("div");
        settingsWrapper.classList.add("unhide", "hidden", "settingsWrapper")

        var platformFamilyToggleWrapper = document.createElement("div");
        platformFamilyToggleWrapper.classList.add("unhide", "hidden", "familyToggleWrapper");

        var unhideAllButton = document.createElement("div");
        unhideAllButton.addEventListener("click", function () { settings.clearHiddenPlatformFamilies(); });
        unhideAllButton.classList.add("unhide", "hidden", "platformFamilyToggleButton");
        unhideAllButton.setAttribute("id", "all-platformFamilyToggleButton");
        unhideAllButton.textContent = "all";
        platformFamilyToggleWrapper.appendChild(unhideAllButton);

        initPlatformsByFamily();
        for (var platformFamily in platformsByFamily) {
            var platformFamilyToggle = document.createElement("div");
            platformFamilyToggle.addEventListener("click", function () {
                settings.toggleHiddenPlatformFamily(this.toString());
            }.bind(platformFamily));
            platformFamilyToggle.classList.add("unhide", "hidden", "platformFamilyToggleButton");
            platformFamilyToggle.setAttribute("id", platformFamily + "-platformFamilyToggleButton");
            platformFamilyToggle.textContent = platformFamily;
            platformFamilyToggleWrapper.appendChild(platformFamilyToggle);
        }
        settingsWrapper.appendChild(platformFamilyToggleWrapper);
        document.body.appendChild(settingsWrapper);

        updateHiddenPlatforms();
        settings.addSettingListener("hiddenPlatformFamilies", updateHiddenPlatforms);
        settings.addSettingListener("enteredSettings", function() { 
            $('html, body').stop().animate({
                scrollTop: $("#metrics-queues-table").offset().top
            }, 200);
        });
    }
}

document.addEventListener("DOMContentLoaded", documentReady);
