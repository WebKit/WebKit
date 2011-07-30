(function() {

var kFastLoadingDEBUG = false;

var g_updateTimerId = 0;
var g_resultsDetailsIterator = null;
var g_treeState = {};

var kBuildFailedInfobarType = 'build-failed';

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

function showResultsDetail(testName, builderName, failureTypeList)
{
    var failureTypes = failureTypeList.join(' ');

    var content = $('.results-detail .content');

    if ($('.failure-details', content).attr(config.kBuilderNameAttr) == builderName &&
        $('.failure-details', content).attr(config.kTestNameAttr) == testName &&
        $('.failure-details', content).attr(config.kFailureTypesAttr) == failureTypes)
        return;

    displayOnButterbar('Loading...');

    results.fetchResultsURLs(builderName, testName, failureTypeList, function(resultsURLs) {
        var status = $('.results-detail .toolbar .status');

        status.empty();
        content.empty();

        // FIXME: We should be passing the full list of failing builder names as the fourth argument.
        status.append(ui.failureDetailsStatus(testName, builderName, failureTypes, [builderName]));
        content.append(ui.failureDetails(resultsURLs));

        toggleButton($('.results-detail .actions .next'), g_resultsDetailsIterator.hasNext());
        toggleButton($('.results-detail .actions .previous'), g_resultsDetailsIterator.hasPrevious());
        toggleButton($('.results-detail .actions .rebaseline'), results.canRebaseline(failureTypeList));

        $('.failure-details', content).attr(config.kBuilderNameAttr, builderName);
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

function selectedFailures()
{
    var failures = [];

    $('.test input:checkbox').each(function() {
        if (!this.checked)
            return;
        var testSummary = $(this).parents('.result');
        var testName = testSummary.attr(config.kTestNameAttr);
        $('.builder', testSummary).each(function() {
            var failureTypes = $(this).attr(config.kFailureTypesAttr);
            if (!failureTypes)
                return
            var failureTypeList = failureTypes.split(' ');
            var builderName = $(this).attr(config.kBuilderNameAttr);
            failures.push({
                'testName': testName,
                'builderName': builderName,
                'failureTypeList': failureTypeList,
            });
        });
    });

    return failures;
}

function rebaselineSelected()
{
    displayOnButterbar('Rebaselining...');
    checkout.rebaselineAll(selectedFailures(), dismissButterbar);
}

function showSelectedFailures()
{
    var argumentList = selectedFailures().map(function(failureDescription) {
        return [failureDescription.testName, failureDescription.builderName, failureDescription.failureTypeList];
    });
    g_resultsDetailsIterator = new base.CallbackIterator(showResultsDetail, argumentList);
    g_resultsDetailsIterator.callNext();
}

function rowsInRevisionRange(earliestRevision, latestRevision)
{
    var earliestRevision = base.asInteger(earliestRevision);
    var latestRevision = base.asInteger(latestRevision);

    var rows = [];
    $('.recent-history tr').each(function() {
        var revision = parseInt($(this).attr(config.kRevisionAttr));
        if (earliestRevision <= revision && revision <= latestRevision)
            rows.push(this);
    });

    return $(rows);
}

function rowsBeforeRevision(latestRevision)
{
    return rowsInRevisionRange(0, latestRevision)
}

function showRecentCommits()
{
    $('.recent-history').empty().append(ui.changelog(model.state.recentCommits));
}

function showBuilderProgress()
{
    $.each(model.state.resultsByBuilder, function(builderName, resultsTree) {
        var builderIndex = config.kBuilders.indexOf(builderName);
        rowsBeforeRevision(resultsTree.revision).each(function() {
            $($(this).children()[builderIndex]).addClass('built');
        });
    });
}

function showUnexpectedFailure(failureAnalysis)
{
    var impliedFirstFailingRevision = failureAnalysis.newestPassingRevision + 1;
    var regressionRows = rowsInRevisionRange(impliedFirstFailingRevision, failureAnalysis.oldestFailingRevision);

    // FIXME: We need to create more rows if regressionRows.length is zero.

    $('.entry', regressionRows).addClass('possible-regression');

    var failureSummary = ui.summarizeFailure(failureAnalysis).attr(config.kRevisionAttr, impliedFirstFailingRevision);
    regressionRows.last().after(failureSummary);

    // FIXME: We should just compute this for failureSummary instead of recomputing the whole page.
    showBuilderProgress();
}

function update()
{
    displayOnButterbar('Loading...');
    base.callInParallel([model.updateRecentCommits, model.updateResultsByBuilder], function() {
        showRecentCommits();
        showBuilderProgress();
        model.analyzeUnexpectedFailures(showUnexpectedFailure);
        dismissButterbar();
    });
}

$('.show-selected-failures').live('click', showSelectedFailures);
$('.rebaseline-selected').live('click', rebaselineSelected);
$('.refresh').live('click', update);

$('.results-detail .actions .next').live('click', nextResultsDetail);
$('.results-detail .actions .previous').live('click', previousResultsDetail);
$('.results-detail .actions .rebaseline').live('click', rebaselineResults);
$('.results-detail .actions .dismiss').live('click', hideResultsDetail);

$(document).ready(function() {
    g_updateTimerId = window.setInterval(update, config.kUpdateFrequency);
    update();
});

})();
