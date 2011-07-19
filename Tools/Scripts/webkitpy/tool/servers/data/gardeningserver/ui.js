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
                    '<th>Test</th><th>Bot</th><th>Regression Range</th><th>Frequency</th>' +
            '</thead>' +
            '<tbody></tbody>' +
        '</table>');
};

ui.summarizeTest = function(testName, resultNodesByBuilder)
{
    var unexpectedResults = results.collectUnexpectedResults(resultNodesByBuilder);
    var block = $(
        '<tr class="test">' +
          '<td class="what"><a draggable></a></td>' +
          '<td class="where"><ul></ul></td>' +
          '<td class="when"></td>' +
          '<td class="how-many"></td>' +
        '</tr>');
    $('.what a', block).text(testName).attr('href', ui.urlForTest(testName)).attr('class', unexpectedResults.join(' '));
    block.attr(config.kTestNameAttr, testName);
    block.attr(config.kFailureTypesAttr, unexpectedResults);

    var where = $('.where', block);
    $.each(resultNodesByBuilder, function(builderName, resultNode) {
        var listElement = $('<li><a href="#"></a></li>');
        where.append(listElement);
        $('a', listElement).attr(config.kBuilderNameAttr, builderName).text(displayNameForBuilder(builderName));
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

    var block = $('<div class="regression-range"><a></a></div>');
    $('a', block).attr('href', href).text(text)
    return block;
};

ui.alertMessageForCompileErrors = function(builderNameList)
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

})();
