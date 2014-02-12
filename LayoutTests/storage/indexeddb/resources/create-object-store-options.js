if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's createObjectStore's various options");

indexedDBTest(prepareDatabase, setVersionComplete);
function prepareDatabase()
{
    db = event.target.result;

    evalAndLog("db.createObjectStore('a', {keyPath: 'a'})");
    evalAndLog("db.createObjectStore('b')");

    debug("db.createObjectStore('c', {autoIncrement: true});");
    db.createObjectStore('c', {autoIncrement: true});
}

function setVersionComplete()
{
    trans = evalAndLog("trans = db.transaction(['a', 'b'], 'readwrite')");
    shouldBeEqualToString("trans.mode", "readwrite");

    req = evalAndLog("trans.objectStore('a').put({'a': 0})");
    req.onsuccess = putB;
    req.onerror = unexpectedErrorCallback;

    evalAndExpectExceptionClass("db.createObjectStore('d', 'bar');", "TypeError");
    evalAndExpectExceptionClass("db.createObjectStore('e', false);", "TypeError");
}

function putB()
{
    req = evalAndLog("trans.objectStore('b').put({'a': 0}, 0)");  // OOPS
    req.onsuccess = getA;
    req.onerror = unexpectedErrorCallback;
}

function getA()
{
    req = evalAndLog("trans.objectStore('a').get(0)");
    req.onsuccess = getB;
    req.onerror = unexpectedErrorCallback;
}

function getB()
{
    shouldBe("event.target.result.a", "{a: 0}");

    req = evalAndLog("trans.objectStore('b').get(0)");
    req.onsuccess = checkB;
    req.onerror = unexpectedErrorCallback;
}

function checkB()
{
    shouldBe("event.target.result.a", "{a: 0}");

    finishJSTest();
}
