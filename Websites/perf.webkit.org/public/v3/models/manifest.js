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

        var tests = [];
        var testParentMap = {};
        for (var testId in rawResponse.tests)
            tests.push(new Test(testId, rawResponse.tests[testId]));

        function buildObjectsFromIdMap(idMap, constructor, resolver) {
            for (var id in idMap) {
                if (resolver)
                    resolver(idMap[id]);
                new constructor(id, idMap[id]);
            }
        }
        buildObjectsFromIdMap(rawResponse.metrics, Metric, function (raw) {
            raw.test = Test.findById(raw.test);
        });

        buildObjectsFromIdMap(rawResponse.all, Platform, function (raw) {
            raw.lastModifiedByMetric = {};
            raw.lastModified.forEach(function (lastModified, index) {
                raw.lastModifiedByMetric[raw.metrics[index]] = lastModified;
            });
            raw.metrics = raw.metrics.map(function (id) { return Metric.findById(id); });
        });
        buildObjectsFromIdMap(rawResponse.builders, Builder);
        buildObjectsFromIdMap(rawResponse.repositories, Repository);
        buildObjectsFromIdMap(rawResponse.bugTrackers, BugTracker, function (raw) {
            if (raw.repositories)
                raw.repositories = raw.repositories.map(function (id) { return Repository.findById(id); });
        });

        Instrumentation.endMeasuringTime('Manifest', '_didFetchManifest');

        return {
            siteTitle: rawResponse.siteTitle,
            dashboards: rawResponse.dashboards, // FIXME: Add an abstraction around dashboards.
        }
    }
}

if (typeof module != 'undefined')
    module.exports.Manifest = Manifest;
