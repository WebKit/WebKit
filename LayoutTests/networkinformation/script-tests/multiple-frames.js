description('Tests using NetworkInfo from multiple frames.');

var bandwidth = 10;
var metered = false;

var connection = navigator.webkitConnection;

function checkNetworkInformation() {
    shouldBe('connection.bandwidth', 'bandwidth');
    shouldBe('connection.metered', 'metered');
}

function checkChildNetworkInformation() {
    shouldBe('connection.bandwidth', 'bandwidth');
    shouldBe('connection.metered', 'metered');
}

var hasMainFrameEventFired = false;
function mainFrameListener() {
    hasMainFrameEventFired = true;
    maybeFinishTest();
}

var hasChildFrameEventFired = false;
function childFrameListener() {
    hasChildFrameEventFired = true;
    maybeFinishTest();
}

function maybeFinishTest() {
    if (hasMainFrameEventFired && hasChildFrameEventFired) {
        checkNetworkInformation();
        checkChildNetworkInformation();
        finishJSTest();
    }
}

var childFrame = document.createElement('iframe');
document.body.appendChild(childFrame);
var childConnection = childFrame.contentWindow.navigator.webkitConnection
childConnection.addEventListener('webkitnetworkinfochange', childFrameListener);
connection.addEventListener('webkitnetworkinfochange', mainFrameListener);

internals.setNetworkInformation(document, 'webkitnetworkinfochange', bandwidth, metered);
window.jsTestIsAsync = true;
