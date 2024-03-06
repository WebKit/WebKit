
class SpinningPage extends Page {
    static htmlTemplate()
    {
        return `<div style="position: absolute; width: 100%; padding-top: 25%; text-align: center;"><spinner-icon></spinner-icon></div>`;
    }
}

function main() {
    const requiredFeatures = {
        'Shadow DOM API': () => { return !!Element.prototype.attachShadow; },
    };

    for (const name in requiredFeatures) {
        if (!requiredFeatures[name]())
            return alert(`Your browser does not support ${name}. Try using the latest Safari or Chrome.`);
    }

    // BEGIN - Speedometer Specific Code
    mockAPIs();
    // END - Speedometer Specific Code

    (new SpinningPage).open();

    Manifest.fetch().then(function (manifest) {
        const dashboardToolbar = new DashboardToolbar;
        const dashboardPages = [];
        if (manifest.dashboards) {
            for (const name in manifest.dashboards)
                dashboardPages.push(new DashboardPage(name, manifest.dashboards[name], dashboardToolbar));
        }

        const summaryPages = [];
        let testFreshnessPage = null;
        if (manifest.summaryPages) {
            for (const summaryPage of manifest.summaryPages)
                summaryPages.push(new SummaryPage(summaryPage));
            testFreshnessPage = new TestFreshnessPage(manifest.summaryPages, manifest.testAgeToleranceInHours);
        }

        const chartsToolbar = new ChartsToolbar;
        const chartsPage = new ChartsPage(chartsToolbar);
        const analysisCategoryPage = new AnalysisCategoryPage();

        if (testFreshnessPage)
            summaryPages.push(testFreshnessPage);

        const createAnalysisTaskPage = new CreateAnalysisTaskPage();
        createAnalysisTaskPage.setParentPage(analysisCategoryPage);

        const analysisTaskPage = new AnalysisTaskPage();
        analysisTaskPage.setParentPage(analysisCategoryPage);

        const buildRequestQueuePage = new BuildRequestQueuePage();
        buildRequestQueuePage.setParentPage(analysisCategoryPage);

        const heading = new Heading(manifest.siteTitle);
        heading.addPageGroup([chartsPage, analysisCategoryPage, ...summaryPages]);

        heading.setTitle(manifest.siteTitle);
        heading.addPageGroup(dashboardPages);

        const router = new PageRouter();
        for (const summaryPage of summaryPages)
            router.addPage(summaryPage);
        router.addPage(chartsPage);
        router.addPage(analysisCategoryPage);
        router.addPage(createAnalysisTaskPage);
        router.addPage(analysisTaskPage);
        router.addPage(buildRequestQueuePage);
        for (const page of dashboardPages)
            router.addPage(page);

        if (summaryPages.length)
            router.setDefaultPage(summaryPages[0]);
        else if (dashboardPages.length)
            router.setDefaultPage(dashboardPages[0]);
        else
            router.setDefaultPage(chartsPage);

        heading.setRouter(router);
        router.route();
    }).catch(function (error) {
        alert('Failed to load the site manifest: ' + error);
    });
}

if (document.readyState != 'loading')
    main();
else
    document.addEventListener('DOMContentLoaded', main);
