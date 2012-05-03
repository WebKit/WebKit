if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("An open connection blocks a separate connection's setVersion call");

connections = []
function test()
{
    removeVendorPrefixes();
    openDBConnection();
}

function openDBConnection()
{
    request = evalAndLog("indexedDB.open('set-version-blocked')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    connection = event.target.result;
    connections.push(connection);
    original_version = connection.version;
    if (connections.length < 2)
        openDBConnection();
    else {
        var versionChangeRequest = evalAndLog("connections[0].setVersion('version 1')");
        versionChangeRequest.onerror = unexpectedErrorCallback;
        versionChangeRequest.onsuccess = inSetVersion;
        versionChangeRequest.onblocked = blocked;
    }
}

seen_blocked_event = false;
function blocked()
{
    evalAndLog("seen_blocked_event = true");
    blocked_event = event;
    shouldBeEqualToString("blocked_event.version", "version 1");
    shouldEvaluateTo("blocked_event.target.readyState", "'pending'");
    evalAndLog("connections[1].close()");
}

function inSetVersion()
{
    debug("in setVersion.onsuccess");
    shouldBeTrue("seen_blocked_event");
    deleteAllObjectStores(connections[0]);
    finishJSTest();
}

test();
