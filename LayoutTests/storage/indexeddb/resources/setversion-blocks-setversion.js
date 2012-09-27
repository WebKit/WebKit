if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that two setVersions from the same connection execute sequentially.");

function test() {
    removeVendorPrefixes();
    setDBNameFromPath();

    evalAndLog("ver = 1");
    evalAndLog("versionChangeComplete = false");

    request = evalAndLog("indexedDB.open(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function h1OpenOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h1 = event.target.result");

        h1.onversionchange = unexpectedVersionChangeCallback;

        request = evalAndLog("h1.setVersion(String(ver++))");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function h1SetVersionOnSuccess1(evt) {
            preamble(event);

            transaction = event.target.result;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function transactionOnComplete1(evt) {
                preamble(evt);
                debug("half done");
                evalAndLog("versionChangeComplete = true");
            };
        };


        request = evalAndLog("h1.setVersion(String(ver++))");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function h1SetVersionOnSuccess2(evt) {
            preamble(event);

            transaction = event.target.result;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function transactionOnComplete2(evt) {
                preamble(evt);
                shouldBeTrue("versionChangeComplete");
                finishJSTest();
            };
        };
    };
}

test();
