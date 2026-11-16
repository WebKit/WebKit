async function makeDomainsPrevalent(...origins)
{
    if (!window.testRunner)
        return;

    testRunner.setStatisticsIsRunningTest(true);
    for (const origin of origins) {
        await testRunner.setStatisticsPrevalentResource(origin, true);
        await testRunner.setStatisticsHasHadUserInteraction(origin, true);
    }
    await testRunner.statisticsUpdateCookieBlocking();

    testRunner.clearMemoryCache();
}

function makeTestSitesPrevalent()
{
    return makeDomainsPrevalent("http://localhost:8000", "http://127.0.0.1:8000");
}

function otherSiteHost()
{
    return location.hostname === "localhost" ? "127.0.0.1" : "localhost";
}

function loadScript(name, host = location.hostname)
{
    const url = new URL(`resources/${name}`, location.href);
    url.hostname = host;
    return new Promise((resolve) => {
        const script = document.createElement("script");
        script.src = url.href;
        script.onload = resolve;
        document.body.appendChild(script);
    });
}

function loadScriptFromOtherSite(name)
{
    return loadScript(name, otherSiteHost());
}
