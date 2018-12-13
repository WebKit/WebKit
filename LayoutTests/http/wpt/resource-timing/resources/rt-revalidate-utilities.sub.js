function createRevalidationURL({tao, crossOrigin}) {
    let token = Math.random();
    let content = encodeURIComponent("var revalidationTest = 1;");
    let mimeType = encodeURIComponent("text/javascript");
    let date = encodeURIComponent(new Date(2000, 1, 1).toGMTString());
    let params = `content=${content}&mimeType=${mimeType}&date=${date}&tao=${tao ? true : false}`;

    const path = "WebKit/resource-timing/resources/rt-revalidation-response.py";
    if (crossOrigin)
        return crossOriginURL(`${token}&${params}`, path);
    return location.origin + `/${path}?${token}&${params}`;
}

function makeRequest(url, revalidation = false) {
    let xhr = new XMLHttpRequest;
    xhr.open("GET", url, true);
    if (revalidation)
        xhr.setRequestHeader("If-Modified-Since", new Date().toGMTString());
    xhr.send();
}

function assertAlways(entry) {
    assert_equals(entry.workerStart, 0, "entry should not have a workerStart time");
    assert_equals(entry.secureConnectionStart, 0, "entry should not have a secureConnectionStart time");

    assert_not_equals(entry.startTime, 0, "entry should have a non-0 fetchStart time");
    assert_not_equals(entry.fetchStart, 0, "entry should have a non-0 startTime time");
    assert_not_equals(entry.responseEnd, 0, "entry should have a non-0 responseEnd time");

    assert_greater_than_equal(entry.fetchStart, entry.startTime, "fetchStart after startTime");
    assert_greater_than_equal(entry.responseEnd, entry.fetchStart, "responseEnd after fetchStart");
}

function assertAllowedTimingData(entry) {
    assert_greater_than_equal(entry.domainLookupStart || entry.fetchStart, entry.fetchStart, "domainLookupStart after fetchStart");
    assert_greater_than_equal(entry.domainLookupEnd || entry.fetchStart, entry.domainLookupStart, "domainLookupEnd after domainLookupStart");
    assert_greater_than_equal(entry.connectStart || entry.fetchStart, entry.domainLookupEnd, "connectStart after domainLookupEnd");
    assert_greater_than_equal(entry.connectEnd || entry.fetchStart, entry.connectStart, "connectEnd after connectStart");
    assert_greater_than_equal(entry.requestStart || entry.fetchStart, entry.connectEnd, "requestStart after connectEnd");
    assert_greater_than_equal(entry.responseStart || entry.fetchStart, entry.requestStart, "responseStart after requestStart");
    assert_greater_than_equal(entry.responseEnd || entry.fetchStart, entry.responseStart, "responseEnd after responseStart");
}

function assertDisallowedTimingData(entry) {
    // These attributes must be zero:
    // https://w3c.github.io/resource-timing/#cross-origin-resources
    const keys = [
        "redirectStart",
        "redirectEnd",
        "domainLookupStart",
        "domainLookupEnd",
        "connectStart",
        "connectEnd",
        "requestStart",
        "responseStart",
        "secureConnectionStart",
    ];
    for (let key of keys)
        assert_equals(entry[key], 0, `entry ${key} must be zero for Cross Origin resource without passing Timing-Allow-Origin check`);
}
