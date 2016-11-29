description('Simulates flow from a page that\'s in the page cache to one that\'s not, and then back to the page cached page.');

onpageshow = function(event) {
    if (window.history.state == 'navigated') {
        testPassed('WebTiming asserts in FrameLoader.cpp did not fire');
        finishJSTest();
    } else {
        if (window.testRunner)
            testRunner.overridePreference('WebKitUsesPageCachePreferenceKey', 1);
        setTimeout(function() {window.history.replaceState('navigated', ''); location.href = 'data:text/html,<script>onunload=function() {},onload=function(){history.back();}<' + '/script>';}, 0);
    }
}

var jsTestIsAsync = true;
