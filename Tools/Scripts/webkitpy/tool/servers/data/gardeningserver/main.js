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

function togglePartyTime(hasFailures)
{
    if (!hasFailures) {
        $('.results').text('No failures. Party time!');
        var partyTime = $('<div class="partytime"><img src="partytime.gif"></div>');
        $('.results').append(partyTime);
        partyTime.fadeIn(1200).delay(7000).fadeOut();
        return;
    }
    $('.results').empty();
}

function ensureResultsSummaryContainer()
{
    var container = $('.results-summary');
    if (container.length)
        return container;
    container = ui.regressionsContainer();
    $('.results').append(container);
    return container;
}

function detachRepairedTestsAndPrepareTestMap(unexpectedFailures)
{
    var testMap = {};

    $('.test').each(function() {
        var testSummary = $(this);
        var testName = testSummary.attr(config.kTestNameAttr);
        if (!(testName in unexpectedFailures))
            testSummary.slideUp(function() { testSummary.detach(); });
        else
            testMap[testName] = testSummary;
    });

    return testMap;
}

function prepareTestSummary(testName, resultNodesByBuilder, callback)
{
    var testSummary = ui.summarizeTest(testName, resultNodesByBuilder);
    var builderNameList = base.keys(resultNodesByBuilder);

    results.unifyRegressionRanges(builderNameList, testName, function(oldestFailingRevision, newestPassingRevision) {
        $('.when', testSummary).append(ui.summarizeRegressionRange(oldestFailingRevision, newestPassingRevision));
    });

    results.countFailureOccurances(builderNameList, testName, function(failureCount) {
        $(testSummary).attr(config.kFailureCountAttr, failureCount);
        $('.how-many', testSummary).text(ui.failureCount(failureCount));
    });

    callback(testSummary);
}

function updateResultsSummary(callback)
{
    results.fetchResultsByBuilder(config.kBuilders, function(resultsByBuilder) {
        var unexpectedFailures = results.unexpectedFailuresByTest(resultsByBuilder);
        var hasFailures = !$.isEmptyObject(unexpectedFailures)

        togglePartyTime(hasFailures);
        setIconState(hasFailures);

        var container = ensureResultsSummaryContainer();
        var testMap = detachRepairedTestsAndPrepareTestMap(unexpectedFailures);

        var newTestSummaries = $();
        var requestTracker = new base.RequestTracker(base.keys(unexpectedFailures).length, function() {
            newTestSummaries.fadeIn();
            callback()
        });

        $.each(unexpectedFailures, function(testName, resultNodesByBuilder) {
            prepareTestSummary(testName, resultNodesByBuilder, function(testSummary) {
                var existingElement = testMap[testName];
                if (existingElement) {
                    existingElement.replaceWith(testSummary);
                    requestTracker.requestComplete();
                    return;
                }
                $('tbody', container).append(testSummary);
                newTestSummaries = newTestSummaries.add(testSummary);
                requestTracker.requestComplete();
            });
        });
    });
}

function showResultsDetail()
{
    var testSummary = $(this).parents('.test');
    var builderName = $(this).attr(config.kBuilderNameAttr);
    var testName = testSummary.attr(config.kTestNameAttr);

    // FIXME: It's lame that we have two different representations of multiple failure types.
    var failureTypes = testSummary.attr(config.kFailureTypesAttr);
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
    displayOnButterbar('Loading...');
    updateResultsSummary(dismissButterbar);
    checkBuilderStatuses();
}

$('.results-summary .where a').live('click', showResultsDetail);
$('.results-detail .actions .dismiss').live('click', hideResultsDetail);
$('.results-detail .actions .rebaseline').live('click', rebaselineResults);

$(document).ready(function() {
    g_updateTimerId = window.setInterval(update, config.kUpdateFrequency);
    update();
});

})();
