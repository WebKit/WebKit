'use strict';

class Manifest {

    static reset()
    {
        AnalysisTask._fetchAllPromise = null;
        AnalysisTask.clearStaticMap();
        BuildRequest.clearStaticMap();
        CommitLog.clearStaticMap();
        Metric.clearStaticMap();
        Platform.clearStaticMap();
        PlatformGroup.clearStaticMap();
        Repository.clearStaticMap();
        CommitSet.clearStaticMap();
        Test.clearStaticMap();
        TestGroup.clearStaticMap();
        Triggerable.clearStaticMap();
        TriggerableConfiguration.clearStaticMap();
        TriggerableRepositoryGroup.clearStaticMap();
        UploadedFile.clearStaticMap();
        BugTracker.clearStaticMap();
        Bug.clearStaticMap();
    }

    static async fetch()
    {
        this.reset();
        const rawManifest = await this.fetchRawResponse();
        return this._didFetchManifest(rawManifest);
    }

    static async fetchRawResponse()
    {
        try {
            return await RemoteAPI.getJSON('/data/manifest.json');
        } catch(error) {
            if (error != 404)
                throw `Failed to fetch manifest.json with ${error}`;
            return await RemoteAPI.getJSON('/api/manifest/');
        }
    }

    static _didFetchManifest(rawResponse)
    {
        Instrumentation.startMeasuringTime('Manifest', '_didFetchManifest');

        const tests = [];
        const testParentMap = {};
        for (let testId in rawResponse.tests)
            tests.push(new Test(testId, rawResponse.tests[testId]));

        function buildObjectsFromIdMap(idMap, constructor, resolver) {
            for (let id in idMap) {
                if (resolver)
                    resolver(idMap[id]);
                new constructor(id, idMap[id]);
            }
        }

        buildObjectsFromIdMap(rawResponse.metrics, Metric, (raw) => {
            raw.test = Test.findById(raw.test);
        });

        buildObjectsFromIdMap(rawResponse.platformGroups, PlatformGroup);
        buildObjectsFromIdMap(rawResponse.all, Platform, (raw) => {
            raw.lastModifiedByMetric = {};
            raw.lastModified.forEach((lastModified, index) => {
                raw.lastModifiedByMetric[raw.metrics[index]] = lastModified;
            });
            raw.metrics = raw.metrics.map((id) => { return Metric.findById(id); });
            raw.group = PlatformGroup.findById(raw.group);
        });
        buildObjectsFromIdMap(rawResponse.builders, Builder);
        buildObjectsFromIdMap(rawResponse.repositories, Repository);
        buildObjectsFromIdMap(rawResponse.bugTrackers, BugTracker, (raw) => {
            if (raw.repositories)
                raw.repositories = raw.repositories.map((id) => { return Repository.findById(id); });
        });
        buildObjectsFromIdMap(rawResponse.triggerables, Triggerable, (raw) => {
            raw.acceptedRepositories = raw.acceptedRepositories.map((repositoryId) => {
                return Repository.findById(repositoryId);
            });
            raw.repositoryGroups = raw.repositoryGroups.map((group) => {
                group.repositories = group.repositories.map((entry) => {
                    return {repository: Repository.findById(entry.repository), acceptsPatch: entry.acceptsPatch};
                });
                return TriggerableRepositoryGroup.ensureSingleton(group.id, group);
            });
            raw.configurations = raw.configurations.map((configuration) => {
                const [testId, platformId, supportedRepetitionTypes] = configuration;
                return {test: Test.findById(testId), platform: Platform.findById(platformId), supportedRepetitionTypes};
            });
        });

        if (typeof(UploadedFile) != 'undefined')
            UploadedFile.fileUploadSizeLimit = rawResponse.fileUploadSizeLimit || 0;

        Instrumentation.endMeasuringTime('Manifest', '_didFetchManifest');

        return {
            siteTitle: rawResponse.siteTitle,
            dashboards: rawResponse.dashboards, // FIXME: Add an abstraction around dashboards.
            summaryPages: rawResponse.summaryPages,
            testAgeToleranceInHours: rawResponse.testAgeToleranceInHours,
            maxRootReuseAgeInDays: rawResponse.maxRootReuseAgeInDays,
        }
    }
}

if (typeof module != 'undefined')
    module.exports.Manifest = Manifest;
