/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

var ui = ui || {};

(function () {

function displayURLForBuilder(builderName)
{
    return 'http://build.chromium.org/p/chromium.webkit/waterfall?' + $.param({
        'builder': builderName
    });
}

function displayNameForBuilder(builderName)
{
    return builderName.replace(/Webkit /, '');
}

function displayNameForRevision(revisionNumber)
{
    return 'r' + revisionNumber;
}

ui.urlForTest = function(testName)
{
    return 'http://trac.webkit.org/browser/trunk/LayoutTests/' + testName;
}

ui.infobarMessageForCompileErrors = function(builderNameList)
{
    var block = $('<div class="compile-errors">Build Failed:<ul></ul></div>');

    var list = $('ul', block);
    $.each(builderNameList, function(index, builderName) {
        var listElement = $('<li><a target="_blank"></a></li>');
        list.append(listElement);
        $('a', listElement).attr('href', displayURLForBuilder(builderName)).text(displayNameForBuilder(builderName));
    });

    return block;
};

ui.failureDetailsStatus = function(failureInfo, builderNameList)
{
    var failureTypes = failureInfo.failureTypeList.join(' ');

    var block = $('<span><span class="test-name"></span><span class="builder-list"></span></span>');
    $('.test-name', block).addClass(failureTypes).text(failureInfo.testName);

    var builderList = $('.builder-list', block);
    $.each(builderNameList, function(index, builderName) {
        var builder = $('<span class="builder-name"></span>')
        builder.text(builderName);
        if (builderName == failureInfo.builderName)
            builder.addClass('selected');
        builderList.append(builder);
    });

    return block;
};

ui.failureDetails = function(resultsURLs)
{
    var block = $('<table class="failure-details"><tbody><tr></tr></tbody></table>');

    if (!resultsURLs.length)
        $('tr', block).append($('<td><div class="missing-data">No data</div></td>'));

    $.each(resultsURLs, function(index, resultURL) {
        var kind = results.resultKind(resultURL);
        var type = results.resultType(resultURL);
        var fragment = (type == results.kImageType) ? '<img>' : '<iframe></iframe>';
        var content = $(fragment).attr('src', resultURL).addClass(kind);
        $('tr', block).append($('<td></td>').append(content));
    });

    return block;
};

function builderTableDataCells(resultNodesByBuilder)
{
    if (!resultNodesByBuilder)
        resultNodesByBuilder = {};

    var list = [];

    $.each(config.kBuilders, function(index, builderName) {
        var block = $('<td class="builder"></td>');
        block.attr('title', displayNameForBuilder(builderName));
        block.attr(config.kBuilderNameAttr, builderName);

        if (builderName in resultNodesByBuilder) {
            var unexpectedFailures = results.unexpectedResults(resultNodesByBuilder[builderName]);
            var failureTypes = unexpectedFailures.join(' ');
            block.attr(config.kFailureTypesAttr, failureTypes);
        }

        list.push(block[0])
    });

    return $(list);
}

ui.summarizeFailure = function(failureAnalysis)
{
    var block = $('<tr class="result"></tr>');
    block.append(builderTableDataCells(failureAnalysis.resultNodesByBuilder));
    block.append(
        '<td class="test">' +
          '<div class="what"><input type="checkbox"> <a class="test-name"></a></div>' +
        '</td></tr>');
    block.attr(config.kTestNameAttr, failureAnalysis.testName);

    var unexpectedResults = results.collectUnexpectedResults(failureAnalysis.resultNodesByBuilder);
    var failureTypes = unexpectedResults.join(' ');
    $('.test-name', block).text(failureAnalysis.testName).attr('href', ui.urlForTest(failureAnalysis.testName)).addClass(failureTypes);

    return block;
};

ui.failureInfoListForSummary = function(testSummary)
{
    var failureInfoList = [];

    var testName = testSummary.attr(config.kTestNameAttr);
    $('.builder', testSummary).each(function() {
        var failureTypes = $(this).attr(config.kFailureTypesAttr);
        if (!failureTypes)
            return
        var failureTypeList = failureTypes.split(' ');
        var builderName = $(this).attr(config.kBuilderNameAttr);
        failureInfoList.push({
            'testName': testName,
            'builderName': builderName,
            'failureTypeList': failureTypeList,
        });
    });

    return failureInfoList;
};

ui.commitEntry = function(commitData)
{
    var entry = $('<td class="entry"></td>');

    entry.append('<div class="details"><a target="_blank" class="revision"></a> <span class="summary"></span> <span class="author"></span> <span class="reviewer-container">(Reviewer: <span class="reviewer"></span>)</span></div><div class="regressions"></div>');
    $('.summary', entry).text(commitData.summary);
    $('.revision', entry).attr('href', trac.changesetURL(commitData.revision)).text(displayNameForRevision(commitData.revision));
    $('.author', entry).text(commitData.author);
    if (commitData.reviewer)
        $('.reviewer', entry).text(commitData.reviewer);
    else
        $('.reviewer-container', entry).detach();
    if (commitData.wasReverted)
        entry.addClass('reverted');

    return entry;
}

ui.changelog = function(commitDataList)
{
    var block = $('<table class="changelog"><tbody></tbody></table>');

    $.each(commitDataList, function(index, commitData) {
        var row = $('<tr class="revision"></tr>');
        row.append(builderTableDataCells());
        row.attr(config.kRevisionAttr, commitData.revision);
        row.append(ui.commitEntry(commitData));
        $('tbody', block).append(row);
    });

    return block;
};

})();
