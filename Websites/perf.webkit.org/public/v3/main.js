
class SpinningPage extends Page {
    static htmlTemplate()
    {
        return `<div style="position: absolute; width: 100%; padding-top: 25%; text-align: center;"><spinner-icon></spinner-icon></div>`;
    }
}

function main() {
    (new SpinningPage).open();

    Manifest.fetch().then(function (manifest) {
        var dashboardToolbar = new DashboardToolbar;
        var dashboardPages = [];
        if (manifest.dashboards) {
            for (var name in manifest.dashboards)
                dashboardPages.push(new DashboardPage(name, manifest.dashboards[name], dashboardToolbar));
        }

        var router = new PageRouter();
        var chartsToolbar = new ChartsToolbar;

        var summaryPage = new SummaryPage(manifest.summary);
        var chartsPage = new ChartsPage(chartsToolbar);
        var analysisCategoryPage = new AnalysisCategoryPage();

        var createAnalysisTaskPage = new CreateAnalysisTaskPage();
        createAnalysisTaskPage.setParentPage(analysisCategoryPage);

        var analysisTaskPage = new AnalysisTaskPage();
        analysisTaskPage.setParentPage(analysisCategoryPage);

        var heading = new Heading(manifest.siteTitle);
        heading.addPageGroup([summaryPage, chartsPage, analysisCategoryPage]);

        heading.setTitle(manifest.siteTitle);
        heading.addPageGroup(dashboardPages);

        var router = new PageRouter();
        router.addPage(summaryPage);
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
    }).catch(function (error) {
        alert('Failed to load the site manifest: ' + error);
    });
}

if (document.readyState != 'loading')
    main();
else
    document.addEventListener('DOMContentLoaded', main);
