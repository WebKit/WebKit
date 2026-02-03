function assertRedirectTimingData(entry) {
    assert_not_equals(entry.redirectStart, 0, "entry should have a redirectStart time");
}

function assertRedirectWithDisallowedTimingData(entry) {
    assert_equals(entry.redirectStart, 0, "entry should not have a redirectStart time");
}

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
    });
    let url = uniqueDataURL("redirect-same-origin-1", "same-origin");
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Same Origin request must have timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-cross-origin-1", "cross-origin");
    url = url + "&pipe=header(Access-Control-Allow-Origin,*)";
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource without Timing-Allow-Origin must have filtered timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-null", "cross-origin");
    url = addTimingAllowOriginHeader(url, "null");
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with Timing-Allow-Origin null value must have filtered timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
    });
    let url = uniqueDataURL("redirect-wildcard", "cross-origin");
    url = addTimingAllowOriginHeader(url, "*");
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with Timing-Allow-Origin wildcard must have timing data");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
    });
    let url = uniqueDataURL("redirect-only-entry", "cross-origin");
    url = addTimingAllowOriginHeader(url, location.origin);
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin in Timing-Allow-Origin list must have timing data (only entry)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
    });
    let url = uniqueDataURL("redirect-middle-entry", "cross-origin");
    url = addCommaSeparatedTimingAllowOriginHeaders(url, ["example.com", location.origin, "example.com"]);
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin in Timing-Allow-Origin list must have timing data (middle entry, comma separated)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectTimingData(entry);
    });
    let url = uniqueDataURL("redirect-middle-entry", "cross-origin");
    url = addMultipleTimingAllowOriginHeaders(url, ["example.com", location.origin, "example.com"]);
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin in Timing-Allow-Origin list must have timing data (middle entry, multiple headers)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-other-origins", "cross-origin");
    url = addCommaSeparatedTimingAllowOriginHeaders(url, [location.origin + ".test", "x" + location.origin]);
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin not in Timing-Allow-Origin list must have filtered timing data (other origins, comma separated)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
    });
    let url = uniqueDataURL("redirect-other-origins", "cross-origin");
    url = addMultipleTimingAllowOriginHeaders(url, [location.origin + ".test", "x" + location.origin]);
    fetch(urlWithRedirectTo(url));
    return promise;
}, "Redirect to Cross Origin resource with origin not in Timing-Allow-Origin list must have filtered timing data (other origins, multiple headers)");

promise_test(function(t) {
    let promise = observeResources(1).then(([entry]) => {
        assertRedirectWithDisallowedTimingData(entry);
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
    });
    let url = uniqueDataURL("multiple-redirect-cross-origin-timing-allowed", "cross-origin");
    url = addTimingAllowOriginHeader(url, location.origin);
    let urlRedirect1 = urlWithRedirectTo(url);
    let urlRedirect2 = urlWithRedirectTo(urlRedirect1);
    let urlRedirect3 = urlWithRedirectTo(urlRedirect2);
    fetch(urlRedirect3);
    return promise;
}, "Multiple level redirect to Cross Origin resource with Timing-Allow-Origin must have must have timing data");
