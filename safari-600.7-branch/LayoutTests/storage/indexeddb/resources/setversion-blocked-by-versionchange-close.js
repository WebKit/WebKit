if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("h2 shouldn't receive any blocked events, and h3 should open after h2 is open");
indexedDBTest(prepareDatabase, openAnother);
function prepareDatabase()
{
    evalAndLog("blockedEventFired = false");
    evalAndLog("versionChangeComplete = false");
    evalAndLog("h2Opened = false");
}

function openAnother(evt)
{
    preamble(evt);
    evalAndLog("h1 = event.target.result");
    h1.onversionchange = unexpectedVersionChangeCallback;
    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onblocked = h2Blocked;
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = h2UpgradeNeeded;
    request.onsuccess = h2Success;

    request = evalAndLog("indexedDB.open(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onsuccess = h3Success;
    evalAndLog("h1.close()");
}

function h2Blocked(evt)
{
    // In multi-process ports h2 can erroneously get a blocked event after
    // finishJSTest is called, which adds extra output and causes the test to
    // fail.
    if (blockedEventFired)
        return;
    preamble(evt);
    evalAndLog("blockedEventFired = true");
}

function h2UpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("h2 = event.target.result");
    h2.onversionchange = unexpectedVersionChangeCallback;
    transaction = event.target.transaction;
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function transactionOnComplete(evt) {
        preamble(evt);
        evalAndLog("versionChangeComplete = true");
    };
}

function h2Success(evt)
{
    preamble(evt);
    evalAndLog("h2Opened = true");
};

function h3Success(evt)
{
    preamble(evt);
    evalAndLog("h3 = event.target.result");
    shouldBe("h3.version", "2");
    debug("FIXME: blocked should not fire as connection was closed. http://webkit.org/b/71130");
    shouldBeFalse("blockedEventFired");
    shouldBeTrue("versionChangeComplete");
    shouldBeTrue("h2Opened");
    finishJSTest();
}
