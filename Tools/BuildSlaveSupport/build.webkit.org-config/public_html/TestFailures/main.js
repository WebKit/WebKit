(function() {

var kFastLoadingDEBUG = false;

var g_updateTimerId = 0;
var g_resultsDetailsIterator = null;

var kBuildFailedInfobarType = 'build-failed';
var kCommitLogLength = 20;

function dismissButterbar()
{
    $('.butterbar').fadeOut('fast');
}

function displayOnButterbar(message)
{
    $('.butterbar .status').text(message);
    $('.butterbar').fadeIn('fast');
}

function hideInfobar()
{
    $('.infobar').animate({
        opacity: 'toggle',
        height: 'toggle',
    });
}

function hideInfobarIfOfType(type)
{
    if (!$('.infobar .status').is(':visible'))
        return;
    if ($('.infobar .status').attr(config.kInfobarTypeAttr) != type)
        return;
    hideInfobar();
}

function displayInfobar(message, type)
{
    $('.infobar .status').empty().attr(config.kInfobarTypeAttr, type).append(message);
    $('.infobar').animate({
        opacity: 'toggle',
        height: 'toggle',
    });
}

function setIconState(hasFailures)
{
    var faviconURL = 'favicon-' + (hasFailures ? 'red' : 'green') + '.png';
    $('#favicon').attr('href', faviconURL);
}

function toggleButton(button, isEnabled)
{
    if (isEnabled)
        button.removeAttr('disabled');
    else
        button.attr('disabled', true)
}

function togglePartyTime(hasFailures)
{
    if (!hasFailures) {
        $('.results .content').text('No failures. Party time!');
        var partyTime = $('<div class="partytime"><img src="partytime.gif"></div>');
        $('.results .content').append(partyTime);
        partyTime.fadeIn(1200).delay(7000).fadeOut();
        return;
    }
    $('.results .content').empty();
}

function ensureResultsSummaryContainer()
{
    var container = $('.results-summary');
    if (container.length)
        return container;
    container = ui.regressionsContainer();
    $('.results .content').append(container);
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

    if (!kFastLoadingDEBUG) {
        results.unifyRegressionRanges(builderNameList, testName, function(oldestFailingRevision, newestPassingRevision) {
            $('.when', testSummary).append(ui.summarizeRegressionRange(oldestFailingRevision, newestPassingRevision));
        });

        results.countFailureOccurances(builderNameList, testName, function(failureCount) {
            $(testSummary).attr(config.kFailureCountAttr, failureCount);
            $('.how-many', testSummary).text(ui.failureCount(failureCount));
        });
    }

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
            $('.results .toolbar').fadeIn();
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

function showResultsDetail(testName, selectedBuilderName, failureTypeListByBuilder)
{
    var builderNameList = base.keys(failureTypeListByBuilder);
    var failureTypeList = failureTypeListByBuilder[selectedBuilderName];
    var failureTypes = failureTypeList.join(' ');

    var content = $('.results-detail .content');

    if ($('.failure-details', content).attr(config.kBuilderNameAttr) == selectedBuilderName &&
        $('.failure-details', content).attr(config.kTestNameAttr) == testName &&
        $('.failure-details', content).attr(config.kFailureTypesAttr) == failureTypes)
        return;

    displayOnButterbar('Loading...');

    results.fetchResultsURLs(selectedBuilderName, testName, failureTypeList, function(resultsURLs) {
        var status = $('.results-detail .toolbar .status');

        status.empty();
        content.empty();

        status.append(ui.failureDetailsStatus(testName, selectedBuilderName, failureTypes, builderNameList));
        content.append(ui.failureDetails(resultsURLs));

        toggleButton($('.results-detail .actions .next'), g_resultsDetailsIterator.hasNext());
        toggleButton($('.results-detail .actions .previous'), g_resultsDetailsIterator.hasPrevious());
        toggleButton($('.results-detail .actions .rebaseline'), results.canRebaseline(failureTypeList));

        $('.failure-details', content).attr(config.kBuilderNameAttr, selectedBuilderName);
        $('.failure-details', content).attr(config.kTestNameAttr, testName);
        $('.failure-details', content).attr(config.kFailureTypesAttr, failureTypes);

        if (!$('.results-detail').is(":visible"))
            $('.results-detail').fadeIn('fast', dismissButterbar);
        else
            dismissButterbar();
    });
}

function hideResultsDetail()
{
    $('.results-detail').fadeOut('fast', function() {
        $('.results-detail .status').empty();
        $('.results-detail .content').empty();
        // Strictly speaking, we don't need to clear g_resultsDetailsIterator,
        // but doing so helps the garbage collector free memory.
        g_resultsDetailsIterator = null;
    });
}

function nextResultsDetail()
{
    g_resultsDetailsIterator.callNext();
}

function previousResultsDetail()
{
    g_resultsDetailsIterator.callPrevious();
}

function resultsDetailArgumentsForBuilderElement(builderBlock)
{
    var selectedBuilderName = builderBlock.attr(config.kBuilderNameAttr);

    var testSummary = builderBlock.parents('.test');
    var testName = testSummary.attr(config.kTestNameAttr);

    // FIXME: It's lame that we have two different representations of multiple failure types.
    var failureTypes = testSummary.attr(config.kFailureTypesAttr);
    var failureTypeList = failureTypes.split(' ');

    var failureTypeListByBuilder = {}
    $('.where .builder-name', testSummary).each(function() {
        var builderName = $(this).attr(config.kBuilderNameAttr);
        // FIXME: We should understand that tests can fail in different ways
        // on different builders.
        failureTypeListByBuilder[builderName] = failureTypeList;
    });

    return [testName, selectedBuilderName, failureTypeListByBuilder];
}

function prepareResultsDetailsIterator(query)
{
    var listOfResultsDetailArguments = [];

    query.each(function() {
        var resultsDetailArguments = resultsDetailArgumentsForBuilderElement($(this));
        listOfResultsDetailArguments.push(resultsDetailArguments);
    });

    return new base.CallbackIterator(showResultsDetail, listOfResultsDetailArguments);
}

function triageFailures()
{
    g_resultsDetailsIterator = prepareResultsDetailsIterator($('.results .test .builder-name'));
    g_resultsDetailsIterator.callNext();
}

function rebaselineSelected()
{
    var rebaselineTasks = [];

    $('.results .test input:checkbox').each(function() {
        if (!this.checked)
            return;
        var testSummary = $(this).parents('.test');
        var testName = testSummary.attr(config.kTestNameAttr);
        $('.builder-name', testSummary).each(function() {
            var builderName = $(this).attr(config.kBuilderNameAttr);
            var failureTypes = $(this).attr(config.kFailureTypesAttr);
            var failureTypeList = failureTypes.split(' ');
            rebaselineTasks.push({
                'builderName': builderName,
                'testName': testName,
                'failureTypeList': failureTypeList,
            });
        });
    });

    displayOnButterbar('Rebaselining...');
    checkout.rebaselineAll(rebaselineTasks, dismissButterbar);
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

function updateRecentCommits()
{
    trac.recentCommitData('trunk', kCommitLogLength, function(commitDataList) {
        var recentHistory  = $('.recent-history');
        recentHistory.empty();
        recentHistory.append(ui.commitLog(commitDataList));
    });
}

function checkBuilderStatuses()
{
    results.fetchBuildersWithCompileErrors(function(builderNameList) {
        if (!builderNameList.length) {
            hideInfobarIfOfType(kBuildFailedInfobarType);
            return;
        }
        displayInfobar(ui.infobarMessageForCompileErrors(builderNameList), kBuildFailedInfobarType);
    });
}

function update()
{
    displayOnButterbar('Loading...');
    updateResultsSummary(dismissButterbar);
    updateRecentCommits();
    checkBuilderStatuses();
}

$('.results .toolbar .triage').live('click', triageFailures);
$('.results .toolbar .rebaseline-selected').live('click', rebaselineSelected);
$('.results-detail .actions .next').live('click', nextResultsDetail);
$('.results-detail .actions .previous').live('click', previousResultsDetail);
$('.results-detail .actions .rebaseline').live('click', rebaselineResults);
$('.results-detail .actions .dismiss').live('click', hideResultsDetail);

$(document).ready(function() {
    g_updateTimerId = window.setInterval(update, config.kUpdateFrequency);
    update();
});

})();
