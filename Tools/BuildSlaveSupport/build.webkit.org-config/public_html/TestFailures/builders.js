var builders = builders || {};

(function() {

var kChromiumBuildBotURL = 'http://build.chromium.org/p/chromium.webkit';

var kUpdateStepName = 'update';
var kUpdateScriptsStepName = 'update_scripts';
var kCompileStepName = 'compile';

function urlForBuildInfo(builderName, buildNumber)
{
    return kChromiumBuildBotURL + '/json/builders/' + encodeURIComponent(builderName) + '/builds/' + encodeURIComponent(buildNumber);
}

function isStepRequredForTestCoverage(step)
{
    switch(step.name) {
    case kUpdateStepName:
    case kUpdateScriptsStepName:
    case kCompileStepName:
        return true;
    default:
        return false;
    }
}

function didFail(step)
{
    // FIXME: Is this the correct way to test for failure?
    return step.results[0] > 0;
}

function didFailStepRequredForTestCoverage(buildInfo)
{
    return buildInfo.steps.filter(isStepRequredForTestCoverage).filter(didFail).length > 0;
}

var g_buildInfoCache = new base.AsynchronousCache(function(key, callback) {
    var explodedKey = key.split('\n');
    $.get(urlForBuildInfo(explodedKey[0], explodedKey[1]), callback);
});

function fetchMostRecentBuildInfoByBuilder(callback)
{
    var buildInfoByBuilder = {};
    var requestTracker = new base.RequestTracker(config.kBuilders.length, callback, [buildInfoByBuilder]);
    $.get(kChromiumBuildBotURL + '/json/builders', function(builderStatus) {
        $.each(config.kBuilders, function(index, builderName) {
            var buildNumber = builderStatus[builderName].cachedBuilds.pop();

            g_buildInfoCache.get(builderName + '\n' + buildNumber, function(buildInfo) {
                buildInfoByBuilder[builderName] = buildInfo;
                requestTracker.requestComplete();
            });
        });
    });
}

builders.buildersFailingStepRequredForTestCoverage = function(callback)
{
    fetchMostRecentBuildInfoByBuilder(function(buildInfoByBuilder) {
        var builderNameList = [];
        $.each(buildInfoByBuilder, function(builderName, buildInfo) {
            if (didFailStepRequredForTestCoverage(buildInfo))
                builderNameList.push(builderName);
        });
        callback(builderNameList);
    });
};

})();
