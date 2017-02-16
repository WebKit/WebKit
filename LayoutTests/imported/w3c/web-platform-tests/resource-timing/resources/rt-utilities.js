let global = this;

function hasNecessaryPerformanceFeatures() {
    return !!(global.PerformanceObserver && global.PerformanceResourceTiming);
}

function testNecessaryPerformanceFeatures() {
    let supported = hasNecessaryPerformanceFeatures();

    test(function() {
        assert_true(supported);
    }, "Must have PerformanceObserver and PerformanceResourceTiming in " + global.constructor.name);

    return supported;
}

function wait() {
    let now = performance.now();
    while (now === performance.now())
        continue;
}

function uniqueDataURL(key, crossOrigin) { return uniqueURL(key, crossOrigin, "resource-timing/resources/data.json"); }
function uniqueImageURL(key, crossOrigin) { return uniqueURL(key, crossOrigin, "resource-timing/resources/resource_timing_test0.png"); }
function uniqueScriptURL(key, crossOrigin) { return uniqueURL(key, crossOrigin, "resource-timing/resources/resource_timing_test0.js"); }
function uniqueStylesheetURL(key, crossOrigin) { return uniqueURL(key, crossOrigin, "resource-timing/resources/resource_timing_test0.css"); }
function uniqueDocumentURL(key, crossOrigin) { return uniqueURL(key, crossOrigin, "resource-timing/resources/resource_timing_test0.html"); }
function uniqueEventSourceURL(key, crossOrigin) { return uniqueURL(key, crossOrigin, "resource-timing/resources/rt-event-stream.py"); }
function uniqueURL(key, crossOrigin, path) {
    let params = key ? `${key}&${Math.random()}` : `${Math.random()}`;
    if (crossOrigin === "cross-origin")
        return `http://{{host}}:{{ports[http][1]}}/${path}?${params}`;
    return location.origin + `/${path}?${params}`;
}

function crossOriginURL(key, path) {
    return `http://{{host}}:{{ports[http][1]}}/${path}?${key}`;
}

function urlWithRedirectTo(url) {
    return `/common/redirect.py?location=${encodeURIComponent(url)}`;
}

function addPipeHeaders(url, headers) {
    return `${url}&pipe=${headers.join("|")}`;
}

function addTimingAllowOriginHeader(url, origin) {
    if (!origin) throw `Invalid origin: ${origin}`;
    return addPipeHeaders(url, [
        `header(Access-Control-Allow-Origin,*)`,
        `header(Timing-Allow-Origin,${origin})`,
    ]);
}

function addMultipleTimingAllowOriginHeaders(url, origins) {
    if (origins.length < 2) throw "Needs >1 origins";
    let allHeaders = [`header(Access-Control-Allow-Origin,*)`];
    allHeaders = allHeaders.concat(origins.map((origin) => `header(Timing-Allow-Origin,${origin},True)`));
    return addPipeHeaders(url, allHeaders);
}

function addCommaSeparatedTimingAllowOriginHeaders(url, origins) {
    if (origins.length < 2) throw "Needs >1 origins";
    return addPipeHeaders(url, [
        `header(Access-Control-Allow-Origin,*)`,
        `header(Timing-Allow-Origin,${origins.join("\\,")})`
    ]);
}

function observeResources(n) {
    return new Promise(function(resolve, reject) {
        let entries = [];
        let observer = new PerformanceObserver(function(list) {
            entries = entries.concat(list.getEntries());
            if (entries.length < n)
                return;
            if (entries.length > n)
                entries = entries.slice(0, n);
            observer.disconnect();
            resolve(entries);
        });
        observer.observe({entryTypes: ["resource"]});
    });
}

function loadResources(n) {
    let observePromise = observeResources(n);
    for (let i = 0; i < n; ++i)
        fetch(uniqueDataURL());
    return observePromise;
}

function restoreEnvironmentToCleanState() {
    performance.clearResourceTimings();
    performance.setResourceTimingBufferSize(200);
    performance.onresourcetimingbufferfull = null;

    assert_equals(performance.getEntriesByType("resource").length, 0, "context should have no resources");
}

function checkResourceTimingEntryType(entry, expected) {
    assert_true(entry instanceof PerformanceResourceTiming, "entry should be a PerformanceResourceTiming instance");
    assert_equals(entry.entryType, "resource", "entryType should be 'resource'");

    if (expected.name)
        assert_equals(entry.name, expected.name, "name should be the url");
    if (expected.initiatorType)
        assert_equals(entry.initiatorType, expected.initiatorType, `initiatorType should be '${expected.initiatorType}'`);
}

function checkResourceTimingEntryTiming(entry) {
    assert_true(entry instanceof PerformanceResourceTiming, "entry should be a PerformanceResourceTiming instance");
    assert_equals(entry.entryType, "resource", "entryType should be 'resource'");

    assert_equals(entry.workerStart, 0, "entry should not have a workerStart time");
    assert_equals(entry.redirectStart, 0, "entry should not have a redirectStart time");
    assert_equals(entry.redirectEnd, 0, "entry should not have a redirectEnd time");
    assert_equals(entry.secureConnectionStart, 0, "entry should not have a secureConnectionStart time");

    assert_not_equals(entry.startTime, 0, "entry should have a non-0 fetchStart time");
    assert_not_equals(entry.fetchStart, 0, "entry should have a non-0 startTime time");
    assert_equals(entry.startTime, entry.fetchStart, "entry startTime should be the same as fetchTime for non-redirect");
    assert_greater_than_equal(entry.domainLookupStart, entry.fetchStart, "domainLookupStart after fetchStart");
    assert_greater_than_equal(entry.domainLookupEnd, entry.domainLookupStart, "domainLookupEnd after domainLookupStart");
    assert_greater_than_equal(entry.connectStart, entry.domainLookupEnd, "connectStart after domainLookupEnd");
    assert_greater_than_equal(entry.connectEnd, entry.connectStart, "connectEnd after connectStart");
    assert_greater_than_equal(entry.requestStart, entry.connectEnd, "requestStart after connectEnd");
    assert_greater_than_equal(entry.responseStart, entry.requestStart, "responseStart after requestStart");
    assert_greater_than_equal(entry.responseEnd, entry.responseStart, "responseEnd after responseStart");
}

function resource_entry_type_test({name, url, initiatorType, generateResource}) {
    let entry = null;

    // Test type attributes.
    promise_test(function(t) {
        let promise = observeResources(1).then(function([observedEntry]) {
            entry = observedEntry;
            checkResourceTimingEntryType(entry, {name: url, initiatorType});
        });
        generateResource(url);
        return promise;
    }, "Type: " + name, {timeout: 3000});

    // Test timing attributes.
    promise_test(function(t) {        
        assert_true(entry !== null, "previous test should have produced an entry");
        checkResourceTimingEntryTiming(entry);
        return Promise.resolve(true);
    }, "Timing: " + name);
}
