importScripts("rt-utilities.js");

addEventListener("message", function(event) {
    if (!hasNecessaryPerformanceFeatures()) {
        postMessage("error");
        return;
    }

    performance.clearResourceTimings();

    let promises = [];

    if (event.data.n)
        promises.push(loadResources(event.data.n));
    if (event.data.sharedResourceURL);
        promises.push(fetch(event.data.sharedResourceURL));

    Promise.all(promises).then(function() {
        let entries = performance.getEntriesByType("resource");
        let serializedEntries = JSON.parse(JSON.stringify(entries));
        postMessage(serializedEntries);
    });
});
