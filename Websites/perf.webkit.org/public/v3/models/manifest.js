'use strict';

class Manifest {

    static fetch()
    {
        return RemoteAPI.getJSON('/data/manifest.json').catch(function () {
            return RemoteAPI.getJSON('/api/manifest/');
        }).then(this._didFetchManifest.bind(this));
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

        buildObjectsFromIdMap(rawResponse.all, Platform, (raw) => {
            raw.lastModifiedByMetric = {};
            raw.lastModified.forEach((lastModified, index) => {
                raw.lastModifiedByMetric[raw.metrics[index]] = lastModified;
            });
            raw.metrics = raw.metrics.map((id) => { return Metric.findById(id); });
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
        });

        Instrumentation.endMeasuringTime('Manifest', '_didFetchManifest');

        return {
            siteTitle: rawResponse.siteTitle,
            dashboards: rawResponse.dashboards, // FIXME: Add an abstraction around dashboards.
            summaryPages: rawResponse.summaryPages,
        }
    }
}

if (typeof module != 'undefined')
    module.exports.Manifest = Manifest;
