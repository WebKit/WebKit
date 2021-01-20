if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test opening twice");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.open('open-twice1')");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = openAnother;
};

function openAnother()
{
    request = evalAndLog("indexedDB.open('open-twice2')");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = finishJSTest;
}

test();
