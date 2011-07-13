(function() {

function dismissButterbar()
{
    $('.butterbar').fadeOut();
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
                    $('.when', testSummary).append(ui.summarizeRegressionRange(oldestFailingRevision, newestPassingRevision));
                });
                results.countFailureOccurances(builderNameList, testName, function(failureCount) {
                    $(testSummary).attr('data-failure-count', failureCount);
                    $('.how-many', testSummary).text(ui.failureCount(failureCount));
                });
            });
            $('.results').append(regressions);
        }
        setIconState(hasFailures);
        onsuccess();
    });
}

$('.butterbar .dismiss').live('click', dismissButterbar);

$(document).ready(function() {
    fetchResults(function() {
        $('.butterbar').fadeOut();
    });
});

})();
