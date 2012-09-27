if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that setVersion is not blocked if handle closed in versionchange handler.");

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
        request.onsuccess = function h1OpenOnSuccess(evt) {
            preamble(evt);
            evalAndLog("h2 = event.target.result");

            h2.onversionchange = function h2OnVersionChange(evt) {
                preamble(evt);
                debug("old = " + JSON.stringify(event.target.version));
                debug("new = " + JSON.stringify(event.version));

                evalAndLog("h2.close()");
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
                    debug("FIXME: blocked should not have fired since connection closed; http://webkit.org/b/71130");
                    shouldBeFalse("blockedEventFired");
                    finishJSTest();
                };
            };
        };
    };
}

test();
