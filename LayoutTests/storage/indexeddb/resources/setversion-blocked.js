if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that setVersion is blocked on open connections.");

function test() {
    removeVendorPrefixes();
    setDBNameFromPath();

    evalAndLog("ver = 1");
    evalAndLog("blockedEventFired = false");

    request = evalAndLog("indexedDB.open(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function h1OpenOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h1 = event.target.result");

        h1.onversionchange = unexpectedVersionChangeCallback;

        request = evalAndLog("indexedDB.open(dbname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function h2OpenOnSuccess(evt) {
            preamble(evt);

            evalAndLog("h2 = event.target.result");
            h2.onversionchange = function h2OnVersionChange(evt) {
                preamble(evt);
                debug("old = " + JSON.stringify(event.target.version));
                debug("new = " + JSON.stringify(event.version));

                debug("scheduling timeout to close h2");
                // This setTimeout() is used to move the close call outside of the event dispatch
                // callback. Per spec, a blocked event should only fire if connections are still
                // open after the versionchange event dispatch is complete.
                // FIXME: There is potential for test flakiness in the timing between when the
                // timeout runs and when the blocked event is dispatched. Move the close call
                // into or after the blocked event handler.
                setTimeout(function timeoutCallback() {
                    preamble();
                    evalAndLog("h2.close()");
                }, 0);
            };

            request = evalAndLog("h1.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function h1SetVersionOnBlocked(evt) {
                preamble(evt);
                evalAndLog("blockedEventFired = true");
            };
            request.onsuccess = function h1SetVersionOnSuccess(evt) {
                preamble(evt);

                transaction = event.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function transactionOnComplete(evt) {
                    preamble(evt);
                    shouldBeTrue("blockedEventFired");
                    finishJSTest();
                };
            };
        };
    };
}

test();
