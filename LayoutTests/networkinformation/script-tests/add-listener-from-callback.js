description('Tests that adding a new event listener from a callback works as expected.');

var bandwidth = 10;
var metered = false;

var connection = navigator.webkitConnection;

function checkNetworkInformation() {
    shouldBe("typeof connection.bandwidth", '"number"');
    shouldBe("typeof connection.metered", '"boolean"');
}

var firstListenerEvents = 0;
function firstListener() {
    checkNetworkInformation();
    if (++firstListenerEvents == 1) {
        connection.addEventListener('webkitnetworkinfochange', secondListener);
        internals.setNetworkInformation(document, 'webkitnetworkinfochange', bandwidth, metered);
    }
    else if (firstListenerEvents > 2)
        testFailed('Too many events for first listener.');
    maybeFinishTest();
}

var secondListenerEvents = 0;
function secondListener() {
    checkNetworkInformation();
    if (++secondListenerEvents > 1)
        testFailed('Too many events for second listener.');
    maybeFinishTest();
}

function maybeFinishTest() {
    if (firstListenerEvents == 2 && secondListenerEvents == 1)
        finishJSTest();
}

connection.addEventListener('webkitnetworkinfochange', firstListener);
internals.setNetworkInformation(document, 'webkitnetworkinfochange', bandwidth, metered);

window.jsTestIsAsync = true;
