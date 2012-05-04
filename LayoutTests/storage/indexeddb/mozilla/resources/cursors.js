// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_cursors.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB cursor behavior");

function test()
{
    removeVendorPrefixes();

    name = self.location.pathname;
    description = "My Test Database";
    request = evalAndLog("indexedDB.open(name, description)");
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
      shouldBe("event.target.result", "null");
      testGroup2();
    }
}

function testGroup2()
{
    objectStore = evalAndLog("db.createObjectStore('autoIncrementKeyPath', { keyPath: 'foo', autoIncrement: true });");

    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      shouldBe("event.target.result", "null");
      testGroup3();
    }
}

function testGroup3()
{
    objectStore = evalAndLog("db.createObjectStore('keyPath', { keyPath: 'foo' });");

    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      shouldBe("event.target.result", "null");
      testGroup4();
    }
}

function testGroup4()
{
    objectStore = evalAndLog("db.createObjectStore('foo');");

    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      shouldBe("event.target.result", "null");
      testGroup5();
    }
}

function testGroup5()
{
    keys = evalAndLog("keys = [1, -1, 0, 10, 2000, 'q', 'z', 'two', 'b', 'a'];");
    sortedKeys = evalAndLog("sortedKeys = [-1, 0, 1, 10, 2000, 'a', 'b', 'q', 'two', 'z'];");

    keyIndex = evalAndLog("keyIndex = 0;");

    for (i in keys) {
      request = evalAndLog("request = objectStore.add('foo', keys[i]);");
      request.onerror = unexpectedErrorCallback;
      request.onsuccess = function(event) {
        if (++keyIndex == keys.length) {
          testGroup6();
        }
      };
    }
}

function testGroup6()
{
    keyIndex = 0;

    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      cursor = evalAndLog("cursor = event.target.result;");
      if (cursor) {
        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        evalAndLog("cursor.continue();");
        evalAndExpectException("cursor.continue();", "DOMException.INVALID_STATE_ERR");

        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        evalAndLog("keyIndex++;");
      }
      else {
        shouldBe("keyIndex", "keys.length");
        testGroup7();
      }
    }
}

function testGroup7()
{
    keyIndex = evalAndLog("keyIndex = 4;");

    range = evalAndLog("range = IDBKeyRange.bound(2000, 'q');");
    request = evalAndLog("request = objectStore.openCursor(range);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      cursor = evalAndLog("cursor = event.target.result;");
      if (cursor) {
        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        evalAndLog("cursor.continue();");

        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        evalAndLog("keyIndex++;");
      }
      else {
        shouldBe("keyIndex", "8");
        testGroup8();
      }
    }
}

function testGroup8()
{
    keyIndex = 0;

    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      cursor = evalAndLog("cursor = event.target.result;");
      if (cursor) {
        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        if (keyIndex) {
          evalAndLog("cursor.continue();");
        }
        else {
          evalAndLog("cursor.continue('b');");
        }

        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        evalAndLog("keyIndex += keyIndex ? 1: 6;");
      }
      else {
        shouldBe("keyIndex", "keys.length");
        testGroup9();
      }
    }
}

function testGroup9()
{
    keyIndex = evalAndLog("keyIndex = 0;");

    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      cursor = evalAndLog("cursor = event.target.result;");
      if (cursor) {
        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        if (keyIndex) {
          evalAndLog("cursor.continue();");
        }
        else {
          evalAndLog("cursor.continue(10);");
        }

        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        evalAndLog("keyIndex += keyIndex ? 1: 3;");
      }
      else {
        shouldBe("keyIndex", "keys.length");
        testGroup10();
      }
    }
}

function testGroup10()
{
    keyIndex = evalAndLog("keyIndex = 0;");

    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      cursor = evalAndLog("cursor = event.target.result;");
      if (cursor) {
        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        if (keyIndex) {
          evalAndLog("cursor.continue();");
        }
        else {
          evalAndLog("cursor.continue('c');");
        }

        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        evalAndLog("keyIndex += keyIndex ? 1 : 7;");
      }
      else {
        shouldBe("keyIndex", "keys.length");
        testGroup11();
      }
    }
}

function testGroup11()
{
    keyIndex = evalAndLog("keyIndex = 0;");

    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      cursor = evalAndLog("cursor = event.target.result;");
      if (cursor) {
        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        if (keyIndex == 4) {
          request = evalAndLog("request = cursor.update('bar');");
          request.onerror = unexpectedErrorCallback;
          request.onsuccess = function(event) {
            evalAndLog("keyIndex++;");
            evalAndLog("cursor.continue();");
          };
        }
        else {
          evalAndLog("keyIndex++;");
          evalAndLog("cursor.continue();");
        }
      }
      else {
        shouldBe("keyIndex", "keys.length");
        testGroup12();
      }
    }
}

function testGroup12()
{
    request = evalAndLog("request = objectStore.get(sortedKeys[4]);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = testGroup12b;
}

function testGroup12b()
{
    shouldBe("event.target.result", "'bar'");

    request = evalAndLog("request = objectStore.put('foo', sortedKeys[4]);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = testGroup13;
}

function testGroup13()
{
    keyIndex = evalAndLog("keyIndex = 0;");

    gotRemoveEvent = false;
    retval = false;

    request = evalAndLog("request = objectStore.openCursor(null, IDBCursor.NEXT);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      cursor = evalAndLog("cursor = event.target.result;");
      if (cursor) {
        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        if (keyIndex == 4) {
          request = evalAndLog("request = cursor.delete();");
          request.onerror = unexpectedErrorCallback;
          request.onsuccess = function(event) {
            shouldBe("keyIndex", "5");
            gotRemoveEvent = evalAndLog("gotRemoveEvent = true;");
          };
        }

        evalAndLog("keyIndex++;");
        evalAndLog("cursor.continue();");
      }
      else {
        shouldBe("keyIndex", "keys.length");
        shouldBe("gotRemoveEvent", "true");
        testGroup14();
      }
    }
}

function testGroup14()
{
    request = evalAndLog("request = objectStore.get(sortedKeys[4]);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = testGroup14b;
}

function testGroup14b()
{
    shouldBe("event.target.result", "undefined");

    request = evalAndLog("request = objectStore.add('foo', sortedKeys[4]);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = testGroup15;
}

function testGroup15()
{
    keyIndex = sortedKeys.length - 1;

    request = evalAndLog("request = objectStore.openCursor(null, IDBCursor.PREV);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
      cursor = evalAndLog("cursor = event.target.result;");
      if (cursor) {
        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        evalAndLog("cursor.continue();");

        shouldBe("cursor.key", "sortedKeys[keyIndex]");
        shouldBe("cursor.primaryKey", "sortedKeys[keyIndex]");
        shouldBe("cursor.value", "'foo'");

        evalAndLog("keyIndex--;");
      }
      else {
        shouldBe("keyIndex", "-1");
        finishJSTest();
      }
    }
}

test();