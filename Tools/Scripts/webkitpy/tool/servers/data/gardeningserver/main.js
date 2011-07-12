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
            var resultsSummary = ui.summarizeResultsByTest(unexpectedFailures);
            $('.results').append($(resultsSummary).addClass('regression'));
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
