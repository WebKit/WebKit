if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that setVersion is not blocked if handle closed in versionchange handler.");

indexedDBTest(prepareDatabase, onOpen);
evalAndLog("blockedEventFired = false");
evalAndLog("versionchangeEventFired = false");
function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("h1 = event.target.result");
    evalAndLog("h1.onversionchange = h1OnVersionChange");
}

function h1OnVersionChange(evt)
{
    preamble(evt);
    evalAndLog("versionchangeEventFired = true");
    shouldBe("event.oldVersion", "1");
    shouldBe("event.newVersion", "2");
    evalAndLog("h1.close()");
}

function onOpen(evt)
{
    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = function h2OpenBlocked(evt) {
        preamble(evt);
        shouldBe("event.oldVersion", "1");
        shouldBe("event.newVersion", "2");
        evalAndLog("blockedEventFired = true");
    };
    request.onupgradeneeded = function h2UpgradeNeeded(evt) {
        preamble(evt);
        shouldBe("event.oldVersion", "1");
        shouldBe("event.newVersion", "2");
    };
    request.onsuccess = function h2OpenSuccess(evt) {
        preamble(evt);
        shouldBeTrue("versionchangeEventFired");
        debug("FIXME: blocked should not have fired since connection closed; http://webkit.org/b/71130");
        shouldBeFalse("blockedEventFired");
        finishJSTest();
    };
}
