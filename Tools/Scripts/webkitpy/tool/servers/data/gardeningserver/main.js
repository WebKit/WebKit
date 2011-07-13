(function() {

function dismissButterbar()
{
    $('.butterbar').fadeOut();
}

function displayOnButterbar(message)
{
    $('.butterbar .status').text(message);
    $('.butterbar').fadeIn();
}

function setIconState(hasFailures)
{
    var faviconURL = 'favicon-' + (hasFailures ? 'red' : 'green') + '.png';
    $('#favicon').attr('href', faviconURL);
}

function fetchResults(onsuccess)
{
    results.fetchResultsByBuilder(config.builders, function(resultsByBuilder) {
        var unexpectedFailures = results.unexpectedFailuresByTest(resultsByBuilder);
        var hasFailures = !$.isEmptyObject(unexpectedFailures)
        if (!hasFailures) {
            $('.results').text('No failures. Party time!');
            var partyTime = $('<div class="partytime"><img src="partytime.gif"></div>');
            $('.results').append(partyTime);
            partyTime.fadeIn(1200).delay(7000).fadeOut();
        } else {
            var regressions = $('<div class="results-summary regression"></div>');
            $.each(unexpectedFailures, function(testName, resultNodesByBuilder) {
                var testSummary = ui.summarizeTest(testName, resultNodesByBuilder);
                regressions.append(testSummary);

                var builderNameList = base.keys(resultNodesByBuilder);
                results.unifyRegressionRanges(builderNameList, testName, function(oldestFailingRevision, newestPassingRevision) {
                    $('.regression-range', testSummary).append(ui.summarizeRegressionRange(oldestFailingRevision, newestPassingRevision));
                    if (!newestPassingRevision)
                        return;
                    checkout.existsAtRevision(checkout.subversionURLForTest(testName), newestPassingRevision, function(testExistedBeforeFailure) {
                        $(testSummary).attr('data-new-test', !testExistedBeforeFailure);
                    });
                });
                results.countFailureOccurances(builderNameList, testName, function(failureCount) {
                    $(testSummary).attr(config.kFailureCountAttr, failureCount);
                    $('.failure-count', testSummary).text(ui.failureCount(failureCount));
                });
            });
            $('.results').append(regressions);
        }
        setIconState(hasFailures);
        onsuccess();
    });
}

function showResultsDetail()
{
    var testBlock = $(this).parents('.test');
    var builderName = $(this).attr(config.kBuilderNameAttr);
    var testName = $('.what', testBlock).text();
    var failureTypeList = testBlock.attr(config.kFailureTypesAttr).split(' ');

    var content = $('.results-detail .content');
    if ($('.results', content).attr(config.kBuilderNameAttr) == builderName && $('.results', content).attr(config.kTestNameAttr) == testName)
        return;

    displayOnButterbar('Loading results');

    results.fetchResultsURLs(builderName, testName, failureTypeList, function(resultsURLs) {
        var status = $('.results-detail .toolbar .status');

        function appendResults() {
            status.text(testName + ' [' + builderName + ']');
            content.append(ui.failureDetails(resultsURLs));
            $('.results', content).attr(config.kBuilderNameAttr, builderName);
            $('.results', content).attr(config.kTestNameAttr, testName);
        }

        var children = content.children();
        if (children.length && $('.results-detail').is(":visible")) {
            // The results-detail pane is already open. Let's do a quick cross-fade.
            status.fadeOut('fast');
            children.fadeOut('fast', function() {
                content.empty();
                appendResults();
                status.fadeIn('fast');
                content.children().hide().fadeIn('fast', dismissButterbar);
            });
        } else {
            appendResults();
            $('.results-detail').fadeIn('fast', dismissButterbar);
        }
    });
}

function hideResultsDetail()
{
    $('.results-detail').fadeOut('fast', function() {
        $('.results-detail .content').empty();
    });
}

$('.butterbar .dismiss').live('click', dismissButterbar);
$('.regression .where li').live('mouseenter', showResultsDetail);
$('.results-detail .dismiss').live('click', hideResultsDetail);

$(document).ready(function() {
    fetchResults(function() {
        $('.butterbar').fadeOut();
    });
});

})();
