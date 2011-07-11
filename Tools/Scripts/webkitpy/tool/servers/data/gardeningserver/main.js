(function() {

function quit()
{
    $.post('/quitquitquit', function(data){
        $('.butterbar .status').html(data)
        $('.butterbar').fadeIn();
    });
}

function hide()
{
    $(this).parent().fadeOut();
}

function setIconState(hasFailures)
{
    var faviconURL = 'favicon-' + (hasFailures ? 'red' : 'green') + '.png';
    $('#favicon').attr('href', faviconURL);
}

function fetchResults(onsuccess)
{
    results.fetchResultsByBuilder(config.builders, function(resultsByBuilder) {
        var unexpectedFailures = ui.summarizeResultsByTest(results.unexpectedFailuresByTest(resultsByBuilder));
        $('.failures').append(unexpectedFailures);
        onsuccess();
    });
    setIconState($('.failures').length);
}

function showResults()
{
    // FIXME: This is fragile.
    var resultsSummary = $(this).parent().parent().parent();
    var testName = $('.testName', resultsSummary).text();
    $('.builderName', resultsSummary).each(function() {
        var builderName = $(this).text();
        results.fetchResultsURLs(builderName, testName, function(resultURLs) {
            resultsSummary.append(ui.results(resultURLs));
        });
    });
}

function findRegressionRange()
{
    // FIXME: This is fragile!
    var builderName = $('.builderName', $(this).parent()).text();
    var testName = $('.testName', $(this).parent().parent().parent()).text();
    results.regressionRangeForFailure(builderName, testName, function(oldestFailingRevision, newestPassingRevision) {
        var tracURLs = [];
        for (var i = newestPassingRevision + 1; i <= oldestFailingRevision; ++i) {
            tracURLs.push('<a href="http://trac.webkit.org/changeset/' + i + '">' + i + '</a>');
        }
        $('.butterbar .status').html('Regression range: ' + tracURLs.join(' '));
        $('.butterbar').fadeIn();
    });
}

$('.hide').live('click', hide);
$('.quit').live('click', quit);
$('.show-results').live('click', showResults);
$('.regression-range').live('click', findRegressionRange);

$(document).ready(function() {
    fetchResults(function() {
        $('.butterbar').fadeOut();
    });
});

})();
