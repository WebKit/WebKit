
class SpinningPage extends Page {
    static htmlTemplate()
    {
        return `<div style="position: absolute; width: 100%; padding-top: 25%; text-align: center;"><spinner-icon></spinner-icon></div>`;
    }
}

function main() {
    const requriedFeatures = {
        'Custom Elements API': () => { return !!window.customElements; },
        'Shadow DOM API': () => { return !!Element.prototype.attachShadow; },
        'Latest DOM': () => { return !!Element.prototype.getRootNode; },
    };

    for (let name in requriedFeatures) {
        if (!requriedFeatures[name]())
            return alert(`Your browser does not support ${name}. Try using the latest Safari or Chrome.`);
    }

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

        var summaryPages = [];
        if (manifest.summaryPages) {
            for (var summaryPage of manifest.summaryPages)
                summaryPages.push(new SummaryPage(summaryPage));
        }

        var chartsPage = new ChartsPage(chartsToolbar);
        var analysisCategoryPage = new AnalysisCategoryPage();

        var createAnalysisTaskPage = new CreateAnalysisTaskPage();
        createAnalysisTaskPage.setParentPage(analysisCategoryPage);

        var analysisTaskPage = new AnalysisTaskPage();
        analysisTaskPage.setParentPage(analysisCategoryPage);

        var buildRequestQueuePage = new BuildRequestQueuePage();
        buildRequestQueuePage.setParentPage(analysisCategoryPage);

        var heading = new Heading(manifest.siteTitle);
        heading.addPageGroup(summaryPages.concat([chartsPage, analysisCategoryPage]));

        heading.setTitle(manifest.siteTitle);
        heading.addPageGroup(dashboardPages);

        var router = new PageRouter();
        for (var summaryPage of summaryPages)
            router.addPage(summaryPage);
        router.addPage(chartsPage);
        router.addPage(createAnalysisTaskPage);
        router.addPage(analysisTaskPage);
        router.addPage(buildRequestQueuePage);
        router.addPage(analysisCategoryPage);
        for (var page of dashboardPages)
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
