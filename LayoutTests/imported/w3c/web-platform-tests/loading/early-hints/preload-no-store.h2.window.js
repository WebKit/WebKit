// META: script=resources/early-hints-helpers.sub.js

const preloads = [{
    "url": "no-store.js?" + Date.now(),
    "as_attr": "script",
}];
fetch_tests_from_window(navigateToTestWithEarlyHints("resources/preload-no-store.html", preloads));
