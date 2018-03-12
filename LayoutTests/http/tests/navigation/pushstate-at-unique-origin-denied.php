<?php
header("Content-Security-Policy: sandbox allow-scripts");
?>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>
<script>
var originURL = document.URL;
test(function () {
    assert_throws('SecurityError', function () {
        history.pushState(null, null, originURL + "/path");
    });
}, 'pushState to a new path in unique origin should fail with SecurityError');

test(function () {
    try {
        history.pushState(null, null, originURL + "#hash");
        done();
    } catch (e) {
        assert_unreached("pushState #hash should not fail.");
    }
}, 'pushState #hash in unique origin should not fail with SecurityError');

test(function () {
    try {
        history.pushState(null, null, originURL + "?query");
        done();
    } catch (e) {
        assert_unreached("pushState ?query should not fail.");
    }
}, 'pushState ?query in unique origin should not fail with SecurityError');

test(function () {
    try {
        history.pushState(null, null, originURL + "?query#hash");
        done();
    } catch (e) {
        assert_unreached("pushState ?query#hash should not fail.");
    }
}, 'pushState ?query#hash in unique origin should not fail with SecurityError');
</script>
