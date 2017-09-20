function assertAlways(entry) {
    assert_equals(entry.workerStart, 0, "entry should not have a workerStart time");
    assert_equals(entry.secureConnectionStart, 0, "entry should not have a secureConnectionStart time");

    assert_not_equals(entry.startTime, 0, "entry should have a non-0 fetchStart time");
    assert_not_equals(entry.fetchStart, 0, "entry should have a non-0 startTime time");
    assert_not_equals(entry.responseEnd, 0, "entry should have a non-0 responseEnd time");

    assert_greater_than_equal(entry.fetchStart, entry.startTime, "fetchStart after startTime");
    assert_greater_than_equal(entry.responseEnd, entry.fetchStart, "responseEnd after fetchStart");
}

function assertRedirectTimingData(entry) {
    assertAlways(entry);
    assert_not_equals(entry.redirectStart, 0, "entry should have a redirectStart time");
    assert_not_equals(entry.redirectEnd, 0, "entry should have a redirectEnd time");
    assert_equals(entry.startTime, entry.redirectStart, "entry startTime should be the same as redirectStart for a redirect");
    assert_greater_than_equal(entry.redirectEnd, entry.redirectStart, "redirectEnd after redirectStart");
    assert_greater_than_equal(entry.fetchStart, entry.redirectEnd, "fetchStart after redirectEnd");
}

function assertRedirectWithDisallowedTimingData(entry) {
    assertAlways(entry);
    assert_equals(entry.redirectStart, 0, "entry should not have a redirectStart time");
    assert_equals(entry.redirectEnd, 0, "entry should not have a redirectEnd time");
    assert_not_equals(entry.startTime, entry.fetchStart, "entry startTime should have matched redirectStart but it was disallowed so it should not match fetchStart");
}

function assertNonRedirectTimingData(entry) {
    assertAlways(entry);
    assert_equals(entry.redirectStart, 0, "entry should not have a redirectStart time");
    assert_equals(entry.redirectEnd, 0, "entry should not have a redirectEnd time");
    assert_equals(entry.startTime, entry.fetchStart, "entry startTime should be the same as fetchTime for non-redirect");
}

function assertAllowedTimingData(entry) {
    assert_greater_than_equal(entry.domainLookupStart, entry.fetchStart, "domainLookupStart after fetchStart");
    assert_greater_than_equal(entry.domainLookupEnd, entry.domainLookupStart, "domainLookupEnd after domainLookupStart");
    assert_greater_than_equal(entry.connectStart, entry.domainLookupEnd, "connectStart after domainLookupEnd");
    assert_greater_than_equal(entry.connectEnd, entry.connectStart, "connectEnd after connectStart");
    assert_greater_than_equal(entry.requestStart, entry.connectEnd, "requestStart after connectEnd");
    assert_greater_than_equal(entry.responseStart, entry.requestStart, "responseStart after requestStart");
    assert_greater_than_equal(entry.responseEnd, entry.responseStart, "responseEnd after responseStart");
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

// No-redirect

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertNonRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("same-origin-1", "same-origin");
    fetch(url);
    return promise;
}, "Same Origin request must have timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertNonRedirectTimingData(entry);
        assertDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("cross-origin-1", "cross-origin");
    url = url + "&pipe=header(Access-Control-Allow-Origin,*)";
    fetch(url);
    return promise;
}, "Cross Origin resource without Timing-Allow-Origin must have filtered timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertNonRedirectTimingData(entry);
        assertDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("null", "cross-origin");
    url = addTimingAllowOriginHeader(url, "null");
    fetch(url);
    return promise;
}, "Cross Origin resource with Timing-Allow-Origin null value must have filtered timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertNonRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("wildcard", "cross-origin");
    url = addTimingAllowOriginHeader(url, "*");
    fetch(url);
    return promise;
}, "Cross Origin resource with Timing-Allow-Origin wildcard must have timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertNonRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("only-entry", "cross-origin");
    url = addTimingAllowOriginHeader(url, location.origin);
    fetch(url);
    return promise;
}, "Cross Origin resource with origin in Timing-Allow-Origin list must have timing data (only entry)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertNonRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("middle-entry", "cross-origin");
    url = addCommaSeparatedTimingAllowOriginHeaders(url, ["example.com", location.origin, "example.com"]);
    fetch(url);
    return promise;
}, "Cross Origin resource with origin in Timing-Allow-Origin list must have timing data (middle entry, comma separated)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertNonRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("middle-entry", "cross-origin");
    url = addMultipleTimingAllowOriginHeaders(url, ["example.com", location.origin, "example.com"]);
    fetch(url);
    return promise;
}, "Cross Origin resource with origin in Timing-Allow-Origin list must have timing data (middle entry, multiple headers)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertNonRedirectTimingData(entry);
        assertDisallowedTimingData(entry); });
    let url = uniqueDataURL("other-origins", "cross-origin");
    url = addCommaSeparatedTimingAllowOriginHeaders(url, [location.origin + ".test", "x" + location.origin]);
    fetch(url);
    return promise;
}, "Cross Origin resource with origin not in Timing-Allow-Origin list must have filtered timing data (other origins, comma separated)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertNonRedirectTimingData(entry);
        assertDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("other-origins", "cross-origin");
    url = addMultipleTimingAllowOriginHeaders(url, [location.origin + ".test", "x" + location.origin]);
    fetch(url);
    return promise;
}, "Cross Origin resource with origin not in Timing-Allow-Origin list must have filtered timing data (other origins, multiple headers)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertNonRedirectTimingData(entry);
        assertDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("case-sensitive", "cross-origin");
    url = addTimingAllowOriginHeader(url, location.origin.toUpperCase());
    fetch(url);
    return promise;
}, "Cross Origin resource with origin not in Timing-Allow-Origin list must have filtered timing data (case-sensitive)");

// Redirects

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-same-origin-1", "same-origin");
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Same Origin request must have timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
        assertDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-cross-origin-1", "cross-origin");
    url = url + "&pipe=header(Access-Control-Allow-Origin,*)";
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource without Timing-Allow-Origin must have filtered timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
        assertDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-null", "cross-origin");
    url = addTimingAllowOriginHeader(url, "null");
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with Timing-Allow-Origin null value must have filtered timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-wildcard", "cross-origin");
    url = addTimingAllowOriginHeader(url, "*");
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with Timing-Allow-Origin wildcard must have timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-only-entry", "cross-origin");
    url = addTimingAllowOriginHeader(url, location.origin);
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin in Timing-Allow-Origin list must have timing data (only entry)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-middle-entry", "cross-origin");
    url = addCommaSeparatedTimingAllowOriginHeaders(url, ["example.com", location.origin, "example.com"]);
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin in Timing-Allow-Origin list must have timing data (middle entry, comma separated)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-middle-entry", "cross-origin");
    url = addMultipleTimingAllowOriginHeaders(url, ["example.com", location.origin, "example.com"]);
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin in Timing-Allow-Origin list must have timing data (middle entry, multiple headers)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
        assertDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-other-origins", "cross-origin");
    url = addCommaSeparatedTimingAllowOriginHeaders(url, [location.origin + ".test", "x" + location.origin]);
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin not in Timing-Allow-Origin list must have filtered timing data (other origins, comma separated)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
        assertDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-other-origins", "cross-origin");
    url = addMultipleTimingAllowOriginHeaders(url, [location.origin + ".test", "x" + location.origin]);
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin not in Timing-Allow-Origin list must have filtered timing data (other origins, multiple headers)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
        assertDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-case-sensitive", "cross-origin");
    url = addTimingAllowOriginHeader(url, location.origin.toUpperCase());
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin not in Timing-Allow-Origin list must have filtered timing data (case-sensitive)");


// Multiple redirects

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("multiple-redirect-same-origin", "same-origin");
    let urlRedirect1 = urlWithRedirectTo(url);
    let urlRedirect2 = urlWithRedirectTo(urlRedirect1);
    let urlRedirect3 = urlWithRedirectTo(urlRedirect2);
    fetch(urlRedirect3);
    return promise;
}, "Multiple level redirect to Same Origin resource must have timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
        assertDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("multiple-redirect-cross-origin", "cross-origin");
    url += "&pipe=header(Access-Control-Allow-Origin,*)";
    let urlRedirect1 = urlWithRedirectTo(url);
    let urlRedirect2 = urlWithRedirectTo(urlRedirect1);
    let urlRedirect3 = urlWithRedirectTo(urlRedirect2);
    fetch(urlRedirect3);
    return promise;
}, "Multiple level redirect to Cross Origin resource without Timing-Allow-Origin must have must have filtered timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
        assertAllowedTimingData(entry);
    });
    let url = uniqueDataURL("multiple-redirect-cross-origin-timing-allowed", "cross-origin");
    url = addTimingAllowOriginHeader(url, location.origin);
    let urlRedirect1 = urlWithRedirectTo(url);
    let urlRedirect2 = urlWithRedirectTo(urlRedirect1);
    let urlRedirect3 = urlWithRedirectTo(urlRedirect2);
    fetch(urlRedirect3);
    return promise;
}, "Multiple level redirect to Cross Origin resource with Timing-Allow-Origin must have must have timing data");

// FIXME: Same Origin -> Cross Origin (no Timing-Allow-Origin) -> Same Origin
// FIXME: Same Origin -> Cross Origin (with Timing-Allow-Origin) -> Same Origin
