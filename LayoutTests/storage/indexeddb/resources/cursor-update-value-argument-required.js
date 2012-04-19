if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB cursor.update required arguments");

function test()
{
    removeVendorPrefixes();

    name = self.location.pathname;
    request = evalAndLog("indexedDB.open(name)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = testGroup1;
    request.onerror = unexpectedErrorCallback;
}

function testGroup1()
{
    deleteAllObjectStores(db);

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

test();