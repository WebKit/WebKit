function checkForPreload(url, shouldPreload) {
    var result = "This test should run in the WebKitTestRunner.";
    if (window.internals) {
        var preloaded = internals.isPreloaded(url);
        if ((shouldPreload && preloaded) || (!shouldPreload && !preloaded))
            result = "PASS ";
        else
            result = "FAIL ";
    }
    document.getElementsByTagName("body")[0].appendChild(document.createTextNode(result));
}
