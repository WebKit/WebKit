description('Tests the basic operation of all NetworkInfo events.');

var bandwidth = 10;
var metered = false;

var connection = navigator.webkitConnection;

function checkNetworkInformation() {
    shouldBe("typeof connection.bandwidth", '"number"');
    shouldBe("typeof connection.metered", '"boolean"');
}

connection.addEventListener('webkitnetworkinfochange', function() {
    debug('webkitnetworkinfochange event is raised');
    checkNetworkInformation();
    finishJSTest();
});

internals.setNetworkInformation(document, 'webkitnetworkinfochange', bandwidth, metered);
window.jsTestIsAsync = true;
