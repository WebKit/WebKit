if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB binary keys");

indexedDBTest(prepareDatabase, testBinaryKeys1);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("db.createObjectStore('store');");
    objectStoreWithKeyPath = evalAndLog("db.createObjectStore('storeWithKeyPath', {keyPath: 'binary'});");
    debug("");
}

function testBinaryKeys1()
{
    preamble();
    evalAndLog("trans = db.transaction('store', 'readwrite')");
    evalAndLog("store = trans.objectStore('store')");

    var n = 0, cases = [
        "[]",
        "[0]",
        "[0, 0]",
        "[0, 1]",
        "[1]",
        "[1, 0]",
        "[1, 1]",
        "[255]",
    ];

    (function testCase() {
        if (!cases.length)
            return;

        while (cases.length) {
            key = cases.shift();
            value = n++;
            debug("");
            evalAndLog("store.put(" + JSON.stringify(value) + ", new Uint8Array(" + key + "));");
        }
    }());

    trans.oncomplete = testBinaryKeys2;
}

const cases = [
    "new Uint8ClampedArray([1,2,3])",
    "new Uint16Array([1,2,3])",
    "new Uint32Array([1,2,3])",
    "new Int8Array([1,2,3])",
    "new Int16Array([1,2,3])",
    "new Int32Array([1,2,3])",
    "new Float32Array([1,2,3])",
    "new Float64Array([1,2,3])",
    "new Uint8Array([1,2,3]).buffer",
    "new DataView(new Uint8Array([1,2,3]).buffer)"
];

function testBinaryKeys2()
{
    preamble();
    evalAndLog("trans = db.transaction('store', 'readwrite')");
    evalAndLog("store = trans.objectStore('store')");

    cases.forEach(function(testCase) {
        debug("");
        evalAndLog("store.put('value', " + testCase + ")");
    });

    trans.oncomplete = testBinaryKeys3;
}

function runTest(testNumber) {
    debug("");
    evalAndLog("binary = " + cases[testNumber]);
    evalAndLog("store.put({ binary })");
    evalAndLog("request = store.get(binary)");
    request.onsuccess = ()=>{
        shouldBeTrue("binary.constructor === request.result.binary.constructor");

        if (++testNumber == cases.length)
            finishJSTest();
        else
            runTest(testNumber);
    }
    request.onerror = unexpectedErrorCallback;
}

function testBinaryKeys3()
{
    preamble();
    evalAndLog("trans = db.transaction('storeWithKeyPath', 'readwrite')");
    evalAndLog("store = trans.objectStore('storeWithKeyPath')");

    runTest(0);
}