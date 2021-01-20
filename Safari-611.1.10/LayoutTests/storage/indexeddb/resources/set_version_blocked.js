if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("An open connection blocks a separate connection's setVersion call");

indexedDBTest(prepareDatabase, openAnother);
function prepareDatabase()
{
    connection = event.target.result;
}

function openAnother()
{
    openRequest = indexedDB.open(dbname, 2);
    openRequest.onblocked = blocked;
    openRequest.onupgradeneeded = inSetVersion;
}

seen_blocked_event = false;
function blocked()
{
    evalAndLog("seen_blocked_event = true");
    blocked_event = event;
    shouldBe("blocked_event.oldVersion", "1");
    shouldBe("blocked_event.newVersion", "2");
    shouldEvaluateTo("blocked_event.target.readyState", "'pending'");
    evalAndLog("connection.close()");
}

function inSetVersion()
{
    debug("in setVersion.onsuccess");
    shouldBeTrue("seen_blocked_event");
    deleteAllObjectStores(connection);
    finishJSTest();
}
