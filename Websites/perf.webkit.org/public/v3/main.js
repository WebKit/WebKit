
class SpinningPage extends Page {
    static htmlTemplate()
    {
        return `<div style="position: absolute; width: 100%; padding-top: 25%; text-align: center;"><spinner-icon></spinner-icon></div>`;
    }
}

function main() {
    (new SpinningPage).open();

    fetchManifest().then(function (manifest) {
        var dashboardToolbar = new DashboardToolbar;
        var dashboardPages = [];
        if (manifest.dashboards) {
            for (var name in manifest.dashboards)
                dashboardPages.push(new DashboardPage(name, manifest.dashboards[name], dashboardToolbar));
        }

        var router = new PageRouter();

        var chartsToolbar = new ChartsToolbar;
        var chartsPage = new ChartsPage(chartsToolbar);

        var analysisCategoryPage = new AnalysisCategoryPage();

        var createAnalysisTaskPage = new CreateAnalysisTaskPage();
        createAnalysisTaskPage.setParentPage(analysisCategoryPage);

        var analysisTaskPage = new AnalysisTaskPage();
        analysisTaskPage.setParentPage(analysisCategoryPage);

        var heading = new Heading(manifest.siteTitle);
        heading.addPageGroup([chartsPage, analysisCategoryPage]);

        heading.setTitle(manifest.siteTitle);
        heading.addPageGroup(dashboardPages);

        var router = new PageRouter();
        router.addPage(chartsPage);
        router.addPage(createAnalysisTaskPage);
        router.addPage(analysisTaskPage);
        router.addPage(analysisCategoryPage);
        for (var page of dashboardPages)
            router.addPage(page);

        if (dashboardPages)
            router.setDefaultPage(dashboardPages[0]);
        else
            router.setDefaultPage(chartsPage);

        heading.setRouter(router);
        router.route();
    });
}

function fetchManifest()
{
    return RemoteAPI.getJSON('../data/manifest.json').then(didFetchManifest, function () {
        return RemoteAPI.getJSON('../api/manifest/').then(didFetchManifest, function (error) {
            alert('Failed to load the site manifest: ' + error);
        });
    });
}

function didFetchManifest(rawResponse)
{
    Instrumentation.startMeasuringTime('Main', 'didFetchManifest');

    var tests = [];
    var testParentMap = {};
    for (var testId in rawResponse.tests) {
        var test = rawResponse.tests[testId];
        var topLevel = !test.parentId;
        if (test.parentId)
            testParentMap[testId] = parseInt(test.parentId);
        tests.push(new Test(testId, test, topLevel));
    }
    for (var testId in testParentMap)
        Test.findById(testId).setParentTest(Test.findById(testParentMap[testId]));

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
        raw.repositories = raw.repositories.map(function (id) { return Repository.findById(id); });
    });

    Instrumentation.endMeasuringTime('Main', 'didFetchManifest');

    return {
        siteTitle: rawResponse.siteTitle,
        dashboards: rawResponse.dashboards, // FIXME: Add an abstraction around dashboards.
    }
}

if (document.readyState != 'loading')
    main();
else
    document.addEventListener('DOMContentLoaded', main);
