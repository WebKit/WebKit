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

function fetchResults(onsuccess)
{
    results.fetchResultsByBuilder(config.builders, function(resultsByBuilder) {
        unexpectedFailures = ui.resultsByTest(results.unexpectedFailuresByTest(resultsByBuilder));
        $('.failures').append(unexpectedFailures);
        onsuccess();
    });
}

$('.hide').live('click', hide);
$('.quit').live('click', quit);

$(document).ready(function() {
    fetchResults(function() {
        $('.butterbar').fadeOut();
    });
});

})();
