function assertAlways(entry) {
    assert_equals(entry.workerStart, 0, "entry should not have a workerStart time");
    assert_equals(entry.secureConnectionStart, 0, "entry should not have a secureConnectionStart time");

    assert_not_equals(entry.startTime, 0, "entry should have a non-0 fetchStart time");
    assert_not_equals(entry.fetchStart, 0, "entry should have a non-0 startTime time");
    assert_not_equals(entry.responseEnd, 0, "entry should have a non-0 responseEnd time");

    assert_greater_than_equal(entry.fetchStart, entry.startTime, "fetchStart after startTime");
    assert_greater_than_equal(entry.responseEnd, entry.fetchStart, "responseEnd after fetchStart");
}

function assertRedirectWithDisallowedTimingData(entry) {
    assertAlways(entry);
    assert_equals(entry.redirectStart, 0, "entry should not have a redirectStart time");
    assert_equals(entry.redirectEnd, 0, "entry should not have a redirectEnd time");
    assert_equals(entry.startTime, entry.fetchStart, "entry startTime should have matched redirectStart but it was disallowed so it should match fetchStart");
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

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
        assertDisallowedTimingData(entry);
    });

    let sameOriginURL = uniqueDataURL("redirect-cross-origin-to-same-origin");
    sameOriginURL = addACAOHeader(sameOriginURL);
    const urlRedirect = urlWithRedirectTo(sameOriginURL);
    const urlWithoutTAO = simpleCrossOriginURLBase() + urlRedirect;
    const url = addTimingAllowOriginHeader(urlWithoutTAO, location.origin);

    fetch(url);
    return promise;
}, "Cross-origin redirection with TAO to same origin loads without TAO must have filtered timing data");
