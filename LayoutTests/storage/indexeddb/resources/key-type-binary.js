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

function testBinaryKeys2()
{
    preamble();
    evalAndLog("trans = db.transaction('store', 'readwrite')");
    evalAndLog("store = trans.objectStore('store')");

    var cases = [
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

    cases.forEach(function(testCase) {
        debug("");
        evalAndLog("store.put('value', " + testCase + ")");
    });

    finishJSTest();
}
