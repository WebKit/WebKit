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

var hasEWS = typeof ews != "undefined";
var EWSCategory = "ews";

var categorizedQueuesByPlatformAndBuildType = {};

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
        if (queue.builder) {
            categoryName = "builders";
        } else if (queue.tester) {
            categoryName = queue.testCategory;
        } else {
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

if (hasEWS) {
    for (var id in ews.queues) {
        var queue = ews.queues[id];
        var platform = categorizedQueuesByPlatformAndBuildType[queue.platform];
        if (!platform)
            platform = categorizedQueuesByPlatformAndBuildType[queue.platform] = {};
        if (!platform.builders)
            platform.builders = {};

        var categoryName = EWSCategory;

        platformQueues = platform[categoryName];
        if (!platformQueues)
            platformQueues = platform[categoryName] = [];

        platformQueues.push(queue);
    }
}

var testNames = {};
testNames[Buildbot.TestCategory.WebKit2] = "WK2 Tests";
testNames[Buildbot.TestCategory.WebKit1] = "WK1 Tests";

function sortedPlatforms()
{
    var platforms = [];

    for (var platformKey in Dashboard.Platform)
        platforms.push(Dashboard.Platform[platformKey]);

    platforms.sort(function(a, b) {
        return a.order - b.order;
    });

    return platforms;
}

function updateHiddenPlatforms()
{
    var hiddenPlatforms = settings.getObject("hiddenPlatforms");
    if (!hiddenPlatforms)
        hiddenPlatforms = [];

    var platformRows = document.querySelectorAll("tr.platform");
    for (var i = 0; i < platformRows.length; ++i)
        platformRows[i].classList.remove("hidden");

    for (var i = 0; i < hiddenPlatforms.length; ++i)
        document.querySelector("tr.platform." + hiddenPlatforms[i]).classList.add("hidden");

    var unhideButton = document.querySelector("div.cellButton.unhide");
    if (hiddenPlatforms.length)
        unhideButton.classList.remove("hidden");
    else
        unhideButton.classList.add("hidden");
}

function documentReady()
{
    var table = document.createElement("table");
    table.classList.add("queue-grid");

    var row = document.createElement("tr");
    row.classList.add("headers");

    var header = document.createElement("th");
    var unhideButton = document.createElement("div");
    unhideButton.addEventListener("click", function () { settings.clearHiddenPlatforms(); });
    unhideButton.textContent = "Show All Platforms";
    unhideButton.classList.add("cellButton", "unhide", "hidden");
    header.appendChild(unhideButton);
    row.appendChild(header);

    header = document.createElement("th");
    header.textContent = "Builders";
    row.appendChild(header);

    for (var testerKey in Buildbot.TestCategory) {
        var header = document.createElement("th");
        header.textContent = testNames[Buildbot.TestCategory[testerKey]];
        row.appendChild(header);
    }

    if (hasEWS) {
        var header = document.createElement("th");
        header.textContent = "EWS";
        row.appendChild(header);
    }

    table.appendChild(row);

    var platforms = sortedPlatforms();

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

        var hideButton = document.createElement("div");
        hideButton.addEventListener("click", function (platformName) { return function () { settings.toggleHiddenPlatform(platformName); }; }(platform.name) );
        hideButton.textContent = "hide";
        hideButton.classList.add("cellButton", "hide");
        cell.appendChild(hideButton);

        row.appendChild(cell);

        cell = document.createElement("td");

        var view = new BuildbotBuilderQueueView(platformQueues.builders.debug, platformQueues.builders.release);
        cell.appendChild(view.element);
        row.appendChild(cell);

        for (var testerKey in Buildbot.TestCategory) {
            var cell = document.createElement("td");

            var testerProperty = Buildbot.TestCategory[testerKey];
            if (platformQueues[testerProperty]) {
                var view = new BuildbotTesterQueueView(platformQueues[testerProperty].debug, platformQueues[testerProperty].release);
                cell.appendChild(view.element);
            }

            row.appendChild(cell);
        }

        if (hasEWS) {
            var cell = document.createElement("td");

            if (platformQueues[EWSCategory]) {
                var view = new EWSQueueView(platformQueues[EWSCategory]);
                cell.appendChild(view.element);
            }

            row.appendChild(cell);
        }

        table.appendChild(row);
    }

    document.body.appendChild(table);

    if (settings.available()) {
        var settingsButton = document.createElement("div");
        settingsButton.addEventListener("click", function () { settings.toggleSettingsDisplay(); });
        settingsButton.classList.add("settings");
        document.body.appendChild(settingsButton);

        updateHiddenPlatforms();
        settings.addSettingListener("hiddenPlatforms", updateHiddenPlatforms);
    }
}

document.addEventListener("DOMContentLoaded", documentReady);
