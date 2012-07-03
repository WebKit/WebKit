description('Tests the basic operation of NetworkInfo.');

var bandwidth = 10;
var metered = false;

var connection = navigator.webkitConnection;

connection.addEventListener('webkitnetworkinfochange', function() {
    shouldBe("typeof connection.bandwidth", '"number"');
    shouldBe("typeof connection.metered", '"boolean"');
    finishJSTest();
});

internals.setNetworkInformation(document, 'webkitnetworkinfochange', bandwidth, metered);

window.jsTestIsAsync = true;
