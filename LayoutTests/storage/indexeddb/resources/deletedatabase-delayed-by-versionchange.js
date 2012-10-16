if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the order when there are pending setVersion, delete and open calls.");

function test() {
    removeVendorPrefixes();
    setDBNameFromPath();

    evalAndLog("ver = 1");
    evalAndLog("setVersionBlockedEventFired = false");
    evalAndLog("versionChangeComplete = false");
    evalAndLog("deleteDatabaseBlockedEventFired = false");
    evalAndLog("deleteDatabaseComplete = false");

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
        };

        request = evalAndLog("indexedDB.open(dbname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function h2OpenOnSuccess(evt) {
            preamble(evt);
            h2 = event.target.result;

            h2.onversionchange = function h2OnVersionChange(evt) {
                preamble(evt);
                debug("old = " + JSON.stringify(event.target.version));
                debug("new = " + JSON.stringify(event.version));
            };

            request = evalAndLog("h.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function setVersionOnBlocked(evt) {
                preamble(evt);
                evalAndLog("setVersionBlockedEventFired = true");

                request = evalAndLog("indexedDB.open(dbname)");
                request.onblocked = unexpectedBlockedCallback;
                request.onerror = unexpectedErrorCallback;
                request.onsuccess = function h3OpenOnSuccess(evt) {
                    preamble(evt);
                    h3 = event.target.result;
                    h3.onversionchange = unexpectedVersionChangeCallback;

                    shouldBeTrue("setVersionBlockedEventFired");
                    shouldBeTrue("versionChangeComplete");
                    shouldBeTrue("deleteDatabaseBlockedEventFired");
                    shouldBeTrue("deleteDatabaseComplete");

                    finishJSTest();
                };

                evalAndLog("h2.close()");
            };
            request.onsuccess = function setVersionOnSuccess(evt) {
                preamble(evt);

                transaction = event.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function transactionOnComplete(evt) {
                    preamble(evt);
                    evalAndLog("versionChangeComplete = true");
                };
            };

            request = evalAndLog("indexedDB.deleteDatabase(dbname)");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function deleteDatabaseOnBlocked(evt) {
                preamble(evt);
                evalAndLog("deleteDatabaseBlockedEventFired = true");

                evalAndLog("h.close()");
            };
            request.onsuccess = function deleteDatabaseOnSuccess(evt) {
                preamble(evt);
                evalAndLog("deleteDatabaseComplete = true");
            };
        };
    };
}

test();
