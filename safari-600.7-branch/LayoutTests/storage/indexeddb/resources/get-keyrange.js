if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's IDBObjectStore.get(IDBKeyRange) method.");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    self.testLength = 10;
    self.objectStore = evalAndLog("db.createObjectStore('someObjectStore')");
    self.index = evalAndLog("objectStore.createIndex('someIndex', 'x')");
    addData();
}

function addData()
{
    for (var i=0; i<self.testLength; i++) {
        evalAndLog("objectStore.add({'x': " + i + " }, " + i + ")");
    }

    evalAndLog("runObjStoreTests()");
}

function runObjStoreTests()
{
    getRangeOnlyTest("objectStore", "get", ".x", "runIndexStoreTests()");
}

function runIndexStoreTests()
{
    getRangeOnlyTest("index",  "get", ".x", "runIndexKeyTests()");
}

function runIndexKeyTests()
{
    getRangeOnlyTest("index", "getKey", "", "finishJSTest()");
}

function getRangeOnlyTest(store, method, resultPath, finish)
{
    request = evalAndLog(store + "." + method + "(IDBKeyRange.only(3))");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function()
    {
        result = event.target.result;
        shouldBe("result" + resultPath, "3");

        getRangeLowerTest(store, method, resultPath, finish);
    };
}

// A closed range with a lower bound should just return that value
function getRangeLowerTest(store, method, resultPath, finish)
{
    request = evalAndLog(store + "." + method + "(IDBKeyRange.lowerBound(5))");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function()
    {
        result = event.target.result;
        shouldBe("result" + resultPath, "5");

        getRangeLowerOpenTest(store, method, resultPath, finish);
    };
}

// An open range with a lower bound should skip the lower bound value
function getRangeLowerOpenTest(store, method, resultPath, finish)
{
    request = evalAndLog(store + "." + method + "(IDBKeyRange.lowerBound(5, true))");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function()
    {
        result = event.target.result;
        shouldBe("result" + resultPath, "6");

        getRangeUpperTest(store, method, resultPath, finish);
    };
}

// range with just upper should just return the first element
function getRangeUpperTest(store, method, resultPath, finish)
{
    request = evalAndLog(store + "." + method + "(IDBKeyRange.upperBound(7))");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function()
    {
        result = event.target.result;
        shouldBe("result" + resultPath, "0");

        getRangeUpperOpenTest(store, method, resultPath, finish);
    };
}

// range with just upper should just return the first element
function getRangeUpperOpenTest(store, method, resultPath, finish)
{
    request = evalAndLog(store + "." + method + "(IDBKeyRange.upperBound(7, true))");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function()
    {
        result = event.target.result;
        shouldBe("result" + resultPath, "0");

        getRangeLowerFractionTest(store, method, resultPath, finish);
    };
}

function getRangeLowerFractionTest(store, method, resultPath, finish)
{
    request = evalAndLog(store + "." + method + "(IDBKeyRange.lowerBound(2.5))");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function()
    {
        result = event.target.result;
        shouldBe("result" + resultPath, "3");

        getOutOfRangeTest(store, method, resultPath, finish);
    };
}

function getOutOfRangeTest(store, method, resultPath, finish)
{
    request = evalAndLog(store + "." + method + "(IDBKeyRange.lowerBound(100))");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function()
    {
        result = event.target.result;
        shouldBe("result", "undefined");

        getBadOnlyTest(store, method, resultPath, finish);
    };
}

function getBadOnlyTest(store, method, resultPath, finish)
{
    request = evalAndLog(store + "." + method + "(IDBKeyRange.only(3.3))");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function()
    {
        result = event.target.result;
        shouldBe("result", "undefined");

        getNullTest(store, method, resultPath, finish);
    };
}

function getNullTest(store, method, resultPath, finish)
{
    evalAndExpectException(store + "." + method + "(null)", "0", "'DataError'");
    evalAndLog(finish);
}
