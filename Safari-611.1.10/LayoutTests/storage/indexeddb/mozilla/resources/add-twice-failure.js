// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_add_twice_failure.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB behavior adding the same property twice");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    key = evalAndLog("key = 10");
    request = evalAndLog("request = objectStore.add({}, key);");
    request.onsuccess = addFirstSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addFirstSuccess()
{
    shouldBe("request.result", "key");
    request = evalAndLog("request = objectStore.add({}, key);");
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = addSecondExpectedError;
}

function addSecondExpectedError()
{
    shouldBe("event.target.error.name", "'ConstraintError'");
    event.preventDefault();
    finishJSTest();
}
