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

ui.urlForRevisionRange = function(firstRevision, lastRevision)
{
    if (firstRevision != lastRevision)
        return 'http://trac.webkit.org/log/trunk/?rev=' + lastRevision + '&stop_rev=' + firstRevision + '&limit=100&verbose=on';
    return 'http://trac.webkit.org/changeset/' + firstRevision;
};

ui.regressionsContainer = function()
{
    return $(
        '<table class="results-summary">' +
            '<thead>' +
                '<tr>' +
                    '<th></th><th>Test</th><th>Bot</th><th>Regression Range</th><th>Frequency</th>' +
            '</thead>' +
            '<tbody></tbody>' +
        '</table>');
};

ui.summarizeTest = function(testName, resultNodesByBuilder)
{
    var unexpectedResults = results.collectUnexpectedResults(resultNodesByBuilder);
    var block = $(
        '<tr class="test">' +
          '<td><input type="checkbox"></td>' +
          '<td class="what"><a class="test-name"></a></td>' +
          '<td class="where"><ul></ul></td>' +
          '<td class="when"></td>' +
          '<td class="how-many"></td>' +
        '</tr>');
    var failureTypes = unexpectedResults.join(' ');
    $('.test-name', block).text(testName).attr('href', ui.urlForTest(testName)).addClass(failureTypes);
    block.attr(config.kTestNameAttr, testName);
    block.attr(config.kFailureTypesAttr, failureTypes);

    var where = $('.where', block);
    $.each(resultNodesByBuilder, function(builderName, resultNode) {
        var listElement = $('<li class="builder-name"></li>');
        listElement.attr(config.kBuilderNameAttr, builderName).attr(config.kFailureTypesAttr, results.unexpectedResults(resultNode)).text(displayNameForBuilder(builderName));
        where.append(listElement);
    });

    return block;
};

ui.summarizeRegressionRange = function(oldestFailingRevision, newestPassingRevision)
{
    if (!oldestFailingRevision || !newestPassingRevision)
        return $('<div class="regression-range">Unknown</div>');

    var impliedFirstFailingRevision = newestPassingRevision + 1;

    var href = ui.urlForRevisionRange(impliedFirstFailingRevision, oldestFailingRevision);
    var text = impliedFirstFailingRevision == oldestFailingRevision ?
        displayNameForRevision(impliedFirstFailingRevision) :
        displayNameForRevision(impliedFirstFailingRevision) + '-' + displayNameForRevision(oldestFailingRevision);

    var block = $('<div class="regression-range"><a target="_blank"></a></div>');
    $('a', block).attr('href', href).text(text)
    return block;
};

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

ui.failureCount = function(failureCount)
{
    if (failureCount < 1)
        return '';
    if (failureCount == 1)
        return 'Seen once.';
    return 'Seen ' + failureCount + ' times.';
};

ui.failureDetailsStatus = function(testName, selectedBuilderName, failureTypes, builderNameList)
{
    var block = $('<span><span class="test-name"></span><span class="builder-list"></span></span>');
    $('.test-name', block).addClass(failureTypes).text(testName);

    var builderList = $('.builder-list', block);
    $.each(builderNameList, function(index, builderName) {
        var builder = $('<span class="builder-name"></span>')
        builder.text(builderName);
        if (builderName == selectedBuilderName)
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

ui.commitLog = function(commitDataList)
{
    var block = $('<div class="changelog"></div>');

    $.each(commitDataList, function(index, commitData) {
        var entry = $('<div class="entry"><div class="summary"></div><div class="details"><a target="_blank" class="revision"></a> <span class="author"></span> <span class="reviewer-container">(Reviewer: <span class="reviewer"></span>)</span></div></div>');
        entry.attr(config.kRevisionAttr, commitData.revision);
        $('.summary', entry).text(commitData.summary);
        $('.revision', entry).attr('href', trac.changesetURL(commitData.revision)).text(displayNameForRevision(commitData.revision));
        $('.author', entry).text(commitData.author);
        if (commitData.reviewer)
            $('.reviewer', entry).text(commitData.reviewer);
        else
            $('.reviewer-container', entry).detach();
        block.append(entry);
    });

    return block;
};

})();
