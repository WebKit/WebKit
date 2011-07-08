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

function expandResults()
{
    var resultsSummary = $(this);
    var testName = $('.testName', resultsSummary).text();
    $('.builderName', resultsSummary).each(function() {
        var builderName = $(this).text();
        results.fetchResultsURLs(builderName, testName, function(resultURLs) {
            resultsSummary.append(ui.results(resultURLs));
        });
    });
}

$('.hide').live('click', hide);
$('.quit').live('click', quit);
$('.results-summary .test').live('click', expandResults);

$(document).ready(function() {
    fetchResults(function() {
        $('.butterbar').fadeOut();
    });
});

})();
