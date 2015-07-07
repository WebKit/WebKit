if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that deleteDatabase is not blocked when connections close in on versionchange callback");

function test() {
    removeVendorPrefixes();
    setDBNameFromPath();

    evalAndLog("blockedEventFired = false");

    request = evalAndLog("indexedDB.open(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function openOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h = event.target.result");

        h.onversionchange = function onVersionChange(evt) {
            preamble(evt);
            shouldBe("event.target.version", "1");
            shouldBe("event.oldVersion", "1");
            shouldBeNull("event.newVersion");
            evalAndLog("h.close()");
        };

        request = evalAndLog("indexedDB.deleteDatabase(dbname)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = function deleteDatabaseOnBlocked(evt) {
            preamble(evt);
            evalAndLog("blockedEventFired = true");
        };
        request.onsuccess = function deleteDatabaseOnSuccess(evt) {
            preamble(evt);
            debug("FIXME: blocked event should not fire since connection closed. http://webkit.org/b/71130");
            shouldBeFalse("blockedEventFired");
            finishJSTest();
        };
    };
}

test();
