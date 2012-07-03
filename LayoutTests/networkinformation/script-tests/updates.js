description('Tests that updates to the connection event causes new events to fire.');

var bandwidth = 10;
var metered = false;

var connection = navigator.webkitConnection;

function checkNetworkInformation() {
    shouldBe("typeof connection.bandwidth", '"number"');
    shouldBe("typeof connection.metered", '"boolean"');
}

function setNetworkInformation() {
    internals.setNetworkInformation(document, 'webkitnetworkinfochange', bandwidth, metered);
}

function firstListener() {
    checkNetworkInformation();
    connection.removeEventListener('webkitnetworkinfochange', firstListener);
    connection.addEventListener('webkitnetworkinfochange', updateListener);
  
    bandwidth = 5;
    metered = true;
    setNetworkInformation();
}

function updateListener(event) {
    checkNetworkInformation(event);
    finishJSTest();
}

connection.addEventListener('webkitnetworkinfochange', firstListener);
setNetworkInformation();
window.jsTestIsAsync = true;
