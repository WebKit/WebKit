if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that open and setVersion are blocked if a VERSION_CHANGE transaction is running, and the connection is closed during the blocked event handler.");

function test() {
    removeVendorPrefixes();
    setDBNameFromPath();

    evalAndLog("ver = 1");
    evalAndLog("blockedEventFired = false");
    evalAndLog("versionChangeComplete = false");
    evalAndLog("errorEventFired = false");

    request = evalAndLog("indexedDB.open(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function h1OpenOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h1 = event.target.result");

        h1.onversionchange = function h1OnVersionChange(evt) {
            preamble(evt);
            debug("old = " + JSON.stringify(event.target.version));
            debug("new = " + JSON.stringify(event.version));
        };

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
                    evalAndLog("versionChangeComplete = true");
                };
            };

            request = evalAndLog("h2.setVersion(String(ver++))");
            request.onblocked = function h2SetVersionOnBlocked(evt) {
                preamble(evt);
                evalAndLog("h2.close()");

                request = evalAndLog("indexedDB.open(dbname)");
                request.onblocked = unexpectedBlockedCallback;
                request.onerror = unexpectedErrorCallback;
                request.onsuccess = function h3OpenOnSuccess(evt) {
                    preamble(event);
                    shouldBeTrue("blockedEventFired");
                    shouldBeTrue("versionChangeComplete");
                    shouldBeTrue("errorEventFired");
                    finishJSTest();
                };
            };
            request.onerror = function h2SetVersionOnError(evt) {
                preamble(evt);
                evalAndLog("errorEventFired = true");
            };
        };
    };
}

test();
