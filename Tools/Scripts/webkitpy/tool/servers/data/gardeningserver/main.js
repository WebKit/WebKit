(function() {

var g_updateTimerId = 0;

var kBuildFailedAlertType = 'build-failed';

function dismissButterbar()
{
    $('.butterbar').fadeOut('fast');
}

function displayOnButterbar(message)
{
    $('.butterbar .status').text(message);
    $('.butterbar').fadeIn();
}

function hideAlert()
{
    $('.alert').animate({
        opacity: 'toggle',
        height: 'toggle',
    });
}

function hideAlertIfOfType(type)
{
    if (!$('.alert .status').is(':visible'))
        return;
    if ($('.alert .status').attr(config.kAlertTypeAttr) != type)
        return;
    hideAlert();
}

function displayAlert(message, type)
{
    $('.alert .status').empty().attr(config.kAlertTypeAttr, type).append(message);
    $('.alert').animate({
        opacity: 'toggle',
        height: 'toggle',
    });
}

function setIconState(hasFailures)
{
    var faviconURL = 'favicon-' + (hasFailures ? 'red' : 'green') + '.png';
    $('#favicon').attr('href', faviconURL);
}

function showResults(onsuccess)
{
    results.fetchResultsByBuilder(config.kBuilders, function(resultsByBuilder) {
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

    // FIXME: It's lame that we have two different representations of multiple failure types.
    var failureTypes = testBlock.attr(config.kFailureTypesAttr);
    var failureTypeList = failureTypes.split(' ');

    var content = $('.results-detail .content');
    if ($('.failure-details', content).attr(config.kBuilderNameAttr) == builderName &&
        $('.failure-details', content).attr(config.kTestNameAttr) == testName &&
        $('.failure-details', content).attr(config.kFailureTypesAttr) == failureTypes)
        return;

    displayOnButterbar('Loading...');

    results.fetchResultsURLs(builderName, testName, failureTypeList, function(resultsURLs) {
        var status = $('.results-detail .toolbar .status');

        function updateResults()
        {
            status.text(testName + ' [' + builderName + ']');
            content.empty();
            content.append(ui.failureDetails(resultsURLs));
            $('.results-detail .actions .rebaseline').toggle(results.canRebaseline(failureTypeList));
            $('.failure-details', content).attr(config.kBuilderNameAttr, builderName);
            $('.failure-details', content).attr(config.kTestNameAttr, testName);
            $('.failure-details', content).attr(config.kFailureTypesAttr, failureTypes);
        }

        var children = content.children();
        if (children.length && $('.results-detail').is(":visible")) {
            // The results-detail pane is already open. Let's do a quick cross-fade.
            status.fadeOut('fast');
            children.fadeOut('fast', function() {
                updateResults();
                status.fadeIn('fast');
                content.children().hide().fadeIn('fast', dismissButterbar);
            });
        } else {
            updateResults();
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

function rebaselineResults()
{
    var failureDetails = $('.failure-details', $(this).parents('.results-detail'));

    var builderName = failureDetails.attr(config.kBuilderNameAttr);
    var testName = failureDetails.attr(config.kTestNameAttr);
    var failureTypes = failureDetails.attr(config.kFailureTypesAttr);
    var failureTypeList = failureTypes.split(' ');

    displayOnButterbar('Rebaselining...');
    checkout.rebaseline(builderName, testName, failureTypeList, dismissButterbar);
}

function checkBuilderStatuses()
{
    results.fetchBuildersWithCompileErrors(function(builderNameList) {
        if (!builderNameList.length) {
            hideAlertIfOfType(kBuildFailedAlertType);
            return;
        }
        displayAlert(ui.alertMessageForCompileErrors(builderNameList), kBuildFailedAlertType);
    });
}

function update()
{
    checkBuilderStatuses();
}

$('.regression .where a').live('click', showResultsDetail);
$('.results-detail .actions .dismiss').live('click', hideResultsDetail);
$('.results-detail .actions .rebaseline').live('click', rebaselineResults);

$(document).ready(function() {
    showResults(function() {
        $('.butterbar').fadeOut();
    });
    g_updateTimerId = window.setInterval(update, config.kUpdateFrequency);
    update();
});

})();
