if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test open and setVersion are blocked if a VERSION_CHANGE transaction is running, and connection closes.");

function test() {
    removeVendorPrefixes();
    setDBNameFromPath();

    evalAndLog("ver = 1");
    evalAndLog("blockedEventFired = false");
    evalAndLog("versionChangeComplete = false");

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

            h2.onversionchange = unexpectedVersionChangeCallback;

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

            request = evalAndLog("indexedDB.open(dbname)");
            request.onblocked = unexpectedBlockedCallback;
            request.onerror = unexpectedErrorCallback;
            request.onsuccess = function h3OpenOnSuccess(evt) {
                preamble(evt);
                debug("FIXME: blocked should not fire as connection was closed. http://webkit.org/b/71130");
                shouldBeFalse("blockedEventFired");
                shouldBeTrue("versionChangeComplete");
                finishJSTest();
            };

            evalAndLog("h2.close()");
        };
    };
}

test();
