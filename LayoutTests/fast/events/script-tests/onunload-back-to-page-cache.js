description('Simulates flow from a page that\'s in the page cache to one that\'s not, and then back to the page cached page.');

onpageshow = function(event) {
    if (window.name == 'navigated') {
        finishJSTest();
    } else {
        if (window.layoutTestController)
            layoutTestController.overridePreference('WebKitUsesPageCachePreferenceKey', 1);
        window.name = 'navigated';
        setTimeout(function() {location.href = 'data:text/html,<script>onunload=function() {},onload=function(){history.back();}<' + '/script>';}, 0);
    }
}

var jsTestIsAsync = true;
var successfullyParsed = true;
