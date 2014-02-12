if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB cursor.update required arguments");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    objectStore = evalAndLog("db.createObjectStore('autoIncrement', { autoIncrement: true });");

    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      testGroup2();
    }
}

function testGroup2()
{
    keys = evalAndLog("keys = [1, -1, 0, 10, 2000, 'q', 'z', 'two', 'b', 'a'];");

    keyIndex = evalAndLog("keyIndex = 0;");

    for (i in keys) {
      request = evalAndLog("request = objectStore.add('foo', keys[i]);");
      request.onerror = unexpectedErrorCallback;
      request.onsuccess = function(event) {
        if (++keyIndex == keys.length) {
          testGroup3();
        }
      };
    }
}

function testGroup3()
{
    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldThrow("cursor.update();");
        } else {
            testFailed("cursor was null");
        }
        finishJSTest();
    }
}
