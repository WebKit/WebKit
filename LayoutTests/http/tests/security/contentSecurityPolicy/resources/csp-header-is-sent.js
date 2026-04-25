async_test(function(t) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
        t.step(function () { assert_true(xhr.status === 404); t.done(); });
    };
    // Send a request that returns 200 if and only if CSP header is present.
    xhr.open(
        "GET",
        "http://127.0.0.1:8000/security/contentSecurityPolicy/resources/test-csp-header.pl",
        true
    );
    xhr.send();
}, 'CSP header is not sent on resource requests for which there is no policy.');

test(function() {
    assert_true(typeof(script_loaded) === "undefined");
}, 'CSP header is not sent on resource requests for which there is a policy.');
