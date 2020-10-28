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
var BubblesCategory = "bubbles";

if (!categorizedQueuesByPlatformAndBuildType)
    var categorizedQueuesByPlatformAndBuildType = {};
var platformsByFamily = {};

for (var i = 0; i < buildbots.length; ++i) {
    var buildbot = buildbots[i];
    for (var id in buildbot.queuesInfo) {
        if (buildbot.queuesInfo[id].combinedQueues) {
            var info = buildbot.queuesInfo[id];
            var queue = {
                id: id,
                branches: info.branches,
                platform: info.platform.name,
                heading: info.heading,
                builder: info.builder,
                combinedQueues: Object.keys(info.combinedQueues).map(function(combinedQueueID) { return buildbot.queues[combinedQueueID]; }),
            };
        } else
            var queue = buildbot.queues[id];

        var platformName = queue.platform;
        var platform = categorizedQueuesByPlatformAndBuildType[platformName];
        if (!platform)
            platform = categorizedQueuesByPlatformAndBuildType[platformName] = {};
        if (!platform.builders)
            platform.builders = [];

        var categoryName;
        if ("combinedQueues" in queue)
            if (queue.builder)
                categoryName = "builderCombinedQueues";
            else
                categoryName = "otherCombinedQueues"
        else if (queue.builder)
            categoryName = "builders";
        else if (queue.tester)
            categoryName = queue.testCategory;
        else if (queue.performance)
            categoryName = "performance";
        else if (queue.leaks)
            categoryName = "leaks";
        else if (queue.staticAnalyzer)
            categoryName = "staticAnalyzer";
        else {
            console.assert("Unknown queue type.");
            continue;
        }

        category = platform[categoryName];
        if (!category)
            category = platform[categoryName] = [];

        category.push(queue);
    }
}

if (hasBubbles) {
    for (var id in bubbleQueueServer.queues) {
        var queue = bubbleQueueServer.queues[id];
        var platform = categorizedQueuesByPlatformAndBuildType[queue.platform];
        if (!platform)
            platform = categorizedQueuesByPlatformAndBuildType[queue.platform] = {};
        if (!platform.builders)
            platform.builders = [];

        var categoryName = BubblesCategory;

        platformQueues = platform[categoryName];
        if (!platformQueues)
            platformQueues = platform[categoryName] = [];

        platformQueues.push(queue);
    }
}

var testNames = {};
testNames[Buildbot.TestCategory.WebKit2] = "WK2 Tests";
testNames[Buildbot.TestCategory.WebKit1] = "WK1 Tests";

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

function updateHiddenPlatforms()
{
    var hiddenPlatformFamilies = settings.getObject("hiddenPlatformFamilies") || [];
    var platformRows = document.querySelectorAll("tr.platform");
    for (var i = 0; i < platformRows.length; ++i)
        platformRows[i].classList.remove("hidden");

    for (var i = 0; i < hiddenPlatformFamilies.length; ++i) {
        var platformFamily = hiddenPlatformFamilies[i];
        if (!(platformFamily in platformsByFamily))
            continue;
        for (var j = 0; j < platformsByFamily[platformFamily].length; ++j) {
            var name = platformsByFamily[platformFamily][j];
            var platformRow = document.querySelector("tr.platform." + name);
            if (platformRow)
                platformRow.classList.add("hidden");
        }
    }
    settings.updateToggleButtons();
}

function applyAccessibilityColorSetting()
{
    var useAccessibleColors = settings.getObject("accessibilityColorsEnabled");
    var toggleAccessibilityColorButton = document.getElementById("accessibilityButton");
    if (useAccessibleColors) {
        toggleAccessibilityColorButton.textContent = "disable accessibility colors";
        document.body.classList.toggle("accessibility-colors");
    } else
        toggleAccessibilityColorButton.textContent = "enable accessibility colors";
}

function toggleAccessibilityColors()
{
    var isCurrentlyActivated = settings.getObject("accessibilityColorsEnabled");
    if (isCurrentlyActivated === undefined)
        isCurrentlyActivated = false;
    
    settings.setObject("accessibilityColorsEnabled", !isCurrentlyActivated);
    document.body.classList.toggle("accessibility-colors");
    var toggleAccessibilityColorButton = document.getElementById("accessibilityButton");
    if (!isCurrentlyActivated)
        toggleAccessibilityColorButton.textContent = "disable accessibility colors";
    else
        toggleAccessibilityColorButton.textContent = "enable accessibility colors";
}

function documentReady()
{
    var table = document.createElement("table");
    table.classList.add("queue-grid");

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

    var header = document.createElement("th");
    header.textContent = "Other";
    row.appendChild(header);

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

        var logoImage = document.createElement("img");
        logoImage.classList.add("logo");
        cell.appendChild(logoImage);

        var ringImage = document.createElement("img");
        ringImage.classList.add("ring");
        ringImage.title = platform.readableName;
        cell.appendChild(ringImage);

        row.appendChild(cell);

        cell = document.createElement("td");
        var view = new BuildbotBuilderQueueView(platformQueues.builders);
        cell.appendChild(view.element);
        row.appendChild(cell);

        if ("builderCombinedQueues" in platformQueues) {
            for (var i = 0; i < platformQueues.builderCombinedQueues.length; ++i) {
                var view = new BuildbotCombinedQueueView(platformQueues.builderCombinedQueues[i]);
                cell.appendChild(view.element);
            }
        }

        for (var testerKey in Buildbot.TestCategory) {
            var cell = document.createElement("td");

            var testerProperty = Buildbot.TestCategory[testerKey];
            if (platformQueues[testerProperty]) {
                var view = new BuildbotTesterQueueView(platformQueues[testerProperty]);
                cell.appendChild(view.element);
            }

            row.appendChild(cell);
        }

        var cell = document.createElement("td");
        if (platformQueues.performance) {
            var view = new BuildbotPerformanceQueueView(platformQueues.performance);
            cell.appendChild(view.element);
        }

        if (platformQueues.staticAnalyzer) {
            var view = new BuildbotStaticAnalyzerQueueView(platformQueues.staticAnalyzer);
            cell.appendChild(view.element);
        }

        if (platformQueues.leaks) {
            var view = new BuildbotLeaksQueueView(platformQueues.leaks);
            cell.appendChild(view.element);
        }

        if (platformQueues.customView)
            cell.appendChild(platformQueues.customView.element);

        if (platformQueues[BubblesCategory]) {
            var view = new BubbleQueueView(platformQueues[BubblesCategory]);
            cell.appendChild(view.element);
        }

        if ("otherCombinedQueues" in platformQueues) {
            for (var i = 0; i < platformQueues.otherCombinedQueues.length; ++i) {
                var view = new BuildbotCombinedQueueView(platformQueues.otherCombinedQueues[i]);
                cell.appendChild(view.element);
            }
        }

        row.appendChild(cell);

        table.appendChild(row);
    }

    document.body.appendChild(table);

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

        var toggleAccessibilityColorButton = document.createElement("div");
        toggleAccessibilityColorButton.addEventListener("click", function() { toggleAccessibilityColors(); });
        toggleAccessibilityColorButton.classList.add("unhide", "hidden", "accessibilityButton");
        toggleAccessibilityColorButton.setAttribute("id", "accessibilityButton");
        toggleAccessibilityColorButton.textContent = "enable accessibility colors";
        settingsWrapper.appendChild(toggleAccessibilityColorButton);
        document.body.appendChild(settingsWrapper);
        applyAccessibilityColorSetting();

        updateHiddenPlatforms();
        settings.addSettingListener("hiddenPlatformFamilies", updateHiddenPlatforms);
    }
}

var sortedRepositories = Dashboard.sortedRepositories;
for (var i = 0; i < sortedRepositories.length; ++i) {
    var trac = sortedRepositories[i].trac;
    if (typeof trac !== "undefined")
        trac.startPeriodicUpdates();
}

document.addEventListener("DOMContentLoaded", documentReady);
