if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Check processing of pending version change requests - same versions.");

indexedDBTest(null, function onConnection1Open(evt) {
    preamble(evt);
    db = event.target.result;

    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onblocked = connection2Blocked;
    request.onupgradeneeded = connection2UpgradeNeeded;
    request.onsuccess = connection2OpenSuccess;
    request.onerror = unexpectedErrorCallback;

    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onsuccess = connection3OpenSuccess;
    request.onerror = unexpectedErrorCallback;
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

function connection2UpgradeNeeded(evt)
{
    preamble(evt);
    shouldBe("event.oldVersion", "1");
    shouldBe("event.newVersion", "2");
}

function connection2OpenSuccess(evt)
{
    preamble(evt);
    evalAndLog("db2 = event.target.result");
    shouldBe("db2.version", "2");
}

function connection3OpenSuccess(evt)
{
    preamble(evt);
    evalAndLog("db3 = event.target.result");
    shouldBe("db3.version", "2");
    finishJSTest();
}
