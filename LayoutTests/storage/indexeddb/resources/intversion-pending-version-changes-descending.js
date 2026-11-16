if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Check processing of pending version change requests - descending versions.");

indexedDBTest(null, function onConnection1Open(evt) {
    preamble(evt);
    db = event.target.result;

    request = evalAndLog("indexedDB.open(dbname, 3)");
    request.onblocked = connection2Blocked;
    request.onupgradeneeded = connection2UpgradeNeeded;
    request.onsuccess = connection2OpenSuccess;
    request.onerror = unexpectedErrorCallback;

    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onblocked = connection3Blocked;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onsuccess = unexpectedErrorCallback;
    request.onerror = connection3Error;
});

function connection2Blocked(evt)
{
    preamble(evt);
    // Attempt to delay this until the third open has been processed;
    // not strictly necessary but will exercise IPC/event timing.
    setTimeout(function() {
        evalAndLog("db.close()");
    }, 0);
}

function connection3Blocked(evt)
{
    preamble(evt);
}

function connection2UpgradeNeeded(evt)
{
    preamble(evt);
    shouldBe("event.oldVersion", "1");
    shouldBe("event.newVersion", "3");
}

function connection2OpenSuccess(evt)
{
    preamble(evt);
    evalAndLog("event.target.result.close()");
    evalAndLog("db2 = event.target.result");
    shouldBe("db2.version", "3");
}

function connection3Error(evt)
{
    preamble(evt);
    shouldBeEqualToString("event.target.error.name", "VersionError");
    finishJSTest();
}
