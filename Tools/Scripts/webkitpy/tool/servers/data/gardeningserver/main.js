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
};

function fetchResults(onsuccess)
{
    results.fetchResultsByBuilder(config.builders, function(resultsByBuilder) {
        var unexpectedFailures = ui.resultsByTest(results.unexpectedFailuresByTest(resultsByBuilder));
        $('.failures').append(unexpectedFailures);
        onsuccess();
    });
    setIconState($('.failures').length);
}

$('.hide').live('click', hide);
$('.quit').live('click', quit);

$(document).ready(function() {
    fetchResults(function() {
        $('.butterbar').fadeOut();
    });
});

})();
