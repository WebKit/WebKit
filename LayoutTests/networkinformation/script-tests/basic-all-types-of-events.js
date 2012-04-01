description('Tests the basic operation of all NetworkInfo events.');

var bandwidth = 10;
var metered = false;

var connection = navigator.webkitConnection;

function checkNetworkInformation() {
    shouldBe('connection.bandwidth', 'bandwidth');
    shouldBe('connection.metered', 'metered');
}

connection.addEventListener('webkitnetworkinfochange', function() {
    debug('webkitnetworkinfochange event is raised');
    checkNetworkInformation();
    finishJSTest();
});

internals.setNetworkInformation(document, 'webkitnetworkinfochange', bandwidth, metered);
window.jsTestIsAsync = true;
