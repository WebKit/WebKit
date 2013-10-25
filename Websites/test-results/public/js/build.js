// FIXME: De-duplicate this code with perf.webkit.org
function TestBuild(repositories, builders, rawRun) {
    const revisions = rawRun.revisions;
    var maxTime = 0;
    var revisionCount = 0;
    for (var repositoryName in revisions) {
        maxTime = Math.max(maxTime, revisions[repositoryName][1]); // Revision is an pair (revision, time)
        revisionCount++;
    }
    var maxTimeString = new Date(maxTime).toISOString().replace('T', ' ').replace(/\.\d+Z$/, '');
    var buildTime = rawRun.buildTime;
    var buildTimeString = new Date(buildTime).toISOString().replace('T', ' ').replace(/\.\d+Z$/, '');

    this.time = function () { return maxTime; }
    this.formattedTime = function () { return maxTimeString; }
    this.buildTime = function () { return buildTime; }
    this.formattedBuildTime = function () { return buildTimeString; }
    this.builder = function () { return builders[rawRun.builder].name; }
    this.buildNumber = function () { return rawRun.buildNumber; }
    this.buildUrl = function () {
        var builderData = builders[rawRun.builder];
        var template = builderData.buildUrl;
        if (!template)
            return null;
        return template.replace(/\$builderName/g, builderData.name).replace(/\$buildNumber/g, this.buildNumber());
    }
    this.revision = function(repositoryId) {
        var repository = revisions[repositoryId];
        return repository ? repository[0] : null;
    }
    this.formattedRevision = function (repositoryId, previousBuild, shouldIncludeNameInLabel) {
        var repository = repositories[repositoryId];
        var repositoryName = repository ? repository.name : 'Unknown repository ' + repositoryId;
        var previousRevision = previousBuild ? previousBuild.revision(repositoryId) : undefined;
        var currentRevision = this.revision(repositoryId);
        if (previousRevision === currentRevision)
            previousRevision = undefined;

        var revisionPrefix = '';
        if (currentRevision.length < 10) { // SVN-like revision.
            revisionPrefix = 'r';
            if (previousRevision)
                previousRevision = (parseInt(previousRevision) + 1);
        }

        var labelForThisRepository = shouldIncludeNameInLabel ? repositoryName : '';
        if (previousRevision) {
            if (labelForThisRepository && shouldIncludeNameInLabel)
                labelForThisRepository += ' ';
            labelForThisRepository += revisionPrefix + previousRevision + '-' + revisionPrefix + currentRevision;
        } else {
            if (shouldIncludeNameInLabel)
                labelForThisRepository += ' @';
            labelForThisRepository += revisionPrefix + currentRevision;
        }

        var url;
        if (repository) {
            if (previousRevision)
                url = (repository['blameUrl'] || '').replace(/\$1/g, previousRevision).replace(/\$2/g, currentRevision);
            else
                url = (repository['url'] || '').replace(/\$1/g, currentRevision);
        }

        return {
            'name': repositoryName,
            'label': labelForThisRepository,
            'currentRevision': currentRevision,
            'previousRevision': previousRevision,
            'url': url,
        };
    }
    this.formattedRevisions = function (previousBuild) {
        var result = {};
        for (var repositoryId in revisions) {
            var info = this.formattedRevision(repositoryId, previousBuild, !!revisionCount);
            result[info.name] = info;
        }
        return result;
    }
}
