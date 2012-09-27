if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that deleteDatabase is blocked on open connections");

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
            debug("old = " + JSON.stringify(event.target.version));
            debug("new = " + JSON.stringify(event.version));

            debug("scheduling timeout to close");
            // This setTimeout() is used to move the close call outside of the event dispatch
            // callback. Per spec, a blocked event should only fire if connections are still
            // open after the versionchange event dispatch is complete.
            // FIXME: There is potential for test flakiness in the timing between when the
            // timeout runs and when the blocked event is dispatched. Move the close call
            // into or after the blocked event handler.
            setTimeout(function timeoutCallback() {
                preamble();
                evalAndLog("h.close()");
            }, 0);
        };

        request = evalAndLog("indexedDB.deleteDatabase(dbname)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = function deleteDatabaseOnBlocked(evt) {
            preamble(evt);
            evalAndLog("blockedEventFired = true");
        };
        request.onsuccess = function deleteDatabaseOnSuccess(evt) {
            preamble(evt);
            shouldBeTrue("blockedEventFired");
            finishJSTest();
        };
    };
}

test();
