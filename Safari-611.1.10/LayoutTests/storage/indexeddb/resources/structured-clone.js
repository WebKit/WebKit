if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test structured clone permutations in IndexedDB.");

indexedDBTest(prepareDatabase, startTests);
function prepareDatabase()
{
    db = event.target.result;
    evalAndLog("store = db.createObjectStore('storeName')");
    debug("This index is not used, but evaluating key path on each put() call will exercise (de)serialization:");
    evalAndLog("store.createIndex('indexName', 'dummyKeyPath')");
}

function startTests()
{
    debug("");
    debug("Running tests...");
    var tests_to_run = [
        testUndefined,
        testNull,
        testBoolean,
        testNumber,
        testString,
        testBigInt,
        testBooleanObject,
        testNumberObject,
        testStringObject,
        testBigIntObject,
        testDateObject,
        testRegExpObject,
        testImageData,
        testBlob,
        testFile,
        testFileList,
        testObject,
        testArray,
        testTypedArray,
        testArrays,
        testMap,
        testSet,
        testGeometryTypes,
        testCryptoKey
    ];

    function nextTest() {
        debug("");
        if (tests_to_run.length) {
            var test = tests_to_run.shift();
            test(nextTest); // When done, call this again.
        } else {
            testBadTypes();
        }
    }
    nextTest();
}

function forEachWithCallback(testFunction, values, callback)
{
    function nextValue() {
        if (values.length) {
            testFunction(values.shift(), nextValue);
        } else {
            callback();
        }
    }

    nextValue();
}

function testValue(value, callback)
{
    // One transaction per test, since some tests require asynchronous
    // operations to verify the result (e.g. via FileReader)
    evalAndLog("transaction = db.transaction('storeName', 'readwrite')");
    transaction.onerror = unexpectedErrorCallback;
    transaction.onabort = unexpectedAbortCallback;
    evalAndLog("store = transaction.objectStore('storeName')");

    self.value = value;
    request = evalAndLog("store.put(value, 'key')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        request = evalAndLog("store.get('key')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function(e) {
            callback(request.result);
        };
    };
}

// Identity testing, sensitive to NaN and -0
function is(x, y) {
    if (x === y) {
        return x !== 0 || 1 / x === 1 / y;
    }
    return x !== x && y !== y;
}

function arrayCompare(a, b) {
    if (a.length !== b.length) {
        return false;
    }
    for (var i = 0; i < a.length; ++i) {
        if (!is(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

function testPrimitiveValue(string, callback)
{
    debug("Testing: " + string);
    var value = eval("value = (" + string + ")");
    test_data = value;
    testValue(test_data, function(result) {
        self.result = result;
        shouldBeTrue("is(test_data, result)");
        callback();
    });
}

function testObjectWithValue(string, callback)
{
    debug("Testing: " + string);
    var value = eval("value = (" + string + ")");
    test_data = value;
    testValue(test_data, function(result) {
        self.result = result;
        shouldBeEqualToString("typeof result", "object");
        shouldBe("Object.prototype.toString.call(result)", "Object.prototype.toString.call(test_data)");
        shouldBeTrue("test_data !== result");
        shouldBe("result.toString()", "test_data.toString()");
        shouldBeTrue("is(test_data.valueOf(), result.valueOf())");
        callback();
    });
}

function testUndefined(callback)
{
    testPrimitiveValue("undefined", callback);
}

function testNull(callback)
{
    testPrimitiveValue("null", callback);
}

function testBoolean(callback)
{
    debug("Testing boolean primitives");
    forEachWithCallback(testPrimitiveValue, ["true", "false"], callback);
}

function testBooleanObject(callback)
{
    debug("Testing Boolean objects");
    forEachWithCallback(testObjectWithValue, [
        "new Boolean(true)",
        "new Boolean(false)"
    ], callback);
}

function testString(callback)
{
    debug("Testing string primitives");
    forEachWithCallback(testPrimitiveValue, [
        "''",
        "'this is a sample string'",
        "'null(\\0)'"
    ], callback);
}

function testStringObject(callback)
{
    debug("Testing String objects");
    forEachWithCallback(testObjectWithValue, [
        "new String()",
        "new String('this is a sample string')",
        "new String('null(\\0)')"
    ], callback);
}

function testNumber(callback)
{
    debug("Testing number primitives");
    forEachWithCallback(testPrimitiveValue, [
        "NaN",
        "-Infinity",
        "-Number.MAX_VALUE",
        "-0xffffffff",
        "-0x80000000",
        "-0x7fffffff",
        "-1",
        "-Number.MIN_VALUE",
        "-0",
        "0",
        "1",
        "Number.MIN_VALUE",
        "0x7fffffff",
        "0x80000000",
        "0xffffffff",
        "Number.MAX_VALUE",
        "Infinity"
    ], callback);
}

function testNumberObject(callback)
{
    debug("Testing Number objects");
    forEachWithCallback(testObjectWithValue, [
        "new Number(NaN)",
        "new Number(-Infinity)",
        "new Number(-Number.MAX_VALUE)",
        "new Number(-Number.MIN_VALUE)",
        "new Number(-0)",
        "new Number(0)",
        "new Number(Number.MIN_VALUE)",
        "new Number(Number.MAX_VALUE)",
        "new Number(Infinity)"
    ], callback);
}

function testBigInt(callback)
{
    debug("Testing BigInt primitives");
    forEachWithCallback(testPrimitiveValue, [
        "-12345678901234567890n",
        "-1n",
        "0n",
        "1n",
        "12345678901234567890n",
    ], callback);
}

function testBigIntObject(callback)
{
    debug("Testing BigInt objects");
    function testOneBigIntObject(string, callback) {
        debug("Testing BigInt object: " + string);
        var value = eval("value = (" + string + ")");
        test_data = value;
        testValue(test_data, function(result) {
            self.result = result;
            shouldBeEqualToString("typeof result", "bigint");
            shouldBe("test_data.toString()", "result.toString()");
            callback();
        });
    }
    forEachWithCallback(testOneBigIntObject, [
        "BigInt(-12345678901234567890)",
        "BigInt(-1)",
        "BigInt(0)",
        "BigInt(1)",
        "BigInt(-0)",
        "BigInt('0x1fffffffffffff')"
    ], callback);
}

function testDateObject(callback)
{
    debug("Testing Date objects");
    forEachWithCallback(testObjectWithValue, [
        "new Date(-1e13)",
        "new Date(-1e12)",
        "new Date(-1e9)",
        "new Date(-1e6)",
        "new Date(-1e3)",
        "new Date(0)",
        "new Date(1e3)",
        "new Date(1e6)",
        "new Date(1e9)",
        "new Date(1e12)",
        "new Date(1e13)"
    ], callback);
}

function testRegExpObject(callback)
{
    debug("Testing RegExp objects");
    function testRegExp(string, callback) {
        debug("Testing RegExp: " + string);
        var value = eval("value = (" + string + ")");
        test_data = value;
        testValue(test_data, function(result) {
            self.result = result;
            shouldBeTrue("test_data !== result");
            shouldBeEqualToString("Object.prototype.toString.call(result)", "[object RegExp]");
            shouldBe("result.toString()", "test_data.toString()");
            debug("");
            callback();
        });
    }

    forEachWithCallback(testRegExp, [
        "new RegExp()",
        "/abc/",
        "/abc/g",
        "/abc/i",
        "/abc/gi",
        "/abc/m",
        "/abc/mg",
        "/abc/mi",
        "/abc/mgi"
    ], callback);
}

function testImageData(callback)
{
    debug("Testing ImageData");
    evalAndLog("canvas = document.createElement('canvas')");
    evalAndLog("canvas.width = 8");
    evalAndLog("canvas.height = 8");
    evalAndLog("test_data = canvas.getContext('2d').getImageData(0, 0, 8, 8)");

    for (var i = 0; i < 256; ++i) {
        test_data.data[i] = i;
    }

    testValue(test_data, function(result) {
        self.result = result;
        shouldBeTrue("test_data !== result");
        shouldBeEqualToString("Object.prototype.toString.call(result)", "[object ImageData]");
        shouldBe("result.width", "test_data.width");
        shouldBe("result.height", "test_data.height");
        shouldBe("result.data.length", "test_data.data.length");
        if (arrayCompare(test_data.data, result.data)) {
            testPassed("result data matches");
        } else {
            testFailed("result data doesn't match");
        }
        callback();
    });
}

function readBlobAsText(blob, callback)
{
    var reader = new FileReader();
    reader.onload = function(e) {
        if (e.target.readyState === FileReader.DONE) {
            callback(e.target.result);
        }
    };
    reader.onerror = function(e) {
        testFailed("Error reading blob as text: " + e);
        finishJSTest();
    };
    reader.readAsText(blob);
}

function checkBlobContents(blob, expected, callback)
{
    readBlobAsText(blob, function(text) {
        self.text = text;
        shouldBeEqualToString("text", expected);
        callback();
    });
}

function compareBlobs(blob1, blob2, callback)
{
    readBlobAsText(blob1, function(text1) {
        readBlobAsText(blob2, function(text2) {
            self.text1 = text1;
            self.text2 = text2;
            shouldBeEqualToString("text2", text1);
            callback();
        });
    });
}

function testBlob(callback)
{
    debug("Testing Blob");
    shouldBeTrue("FileReader != null");
    evalAndLog("test_content = 'This is a test. This is only a test.'");
    evalAndLog("test_data = new Blob([test_content])");
    testValue(test_data, function(result) {
        self.result = result;
        shouldBeTrue("test_data !== result");
        shouldBeEqualToString("Object.prototype.toString.call(result)", "[object Blob]");
        shouldBe("result.size", "test_data.size");
        shouldBe("result.type", "test_data.type");
        checkBlobContents(result, test_content, callback);
    });
}

function compareFiles(file1, file2, callback)
{
    self.file1 = file1;
    self.file2 = file2;
    shouldBeTrue("file1 !== file2");
    shouldBeEqualToString("Object.prototype.toString.call(file1)", "[object File]");
    shouldBeEqualToString("Object.prototype.toString.call(file2)", "[object File]");
    shouldBe("file1.name", "file2.name");
    shouldBe("String(file1.lastModifiedDate)", "String(file2.lastModifiedDate)");
    if (callback) {
        compareBlobs(file1, file2, callback);
    }
}

function testFile(callback)
{
    debug("Testing File");
    evalAndLog("test_content = 'This is a test. This is only a test.'");
    evalAndLog("blob = new Blob([test_content])");
    evalAndLog("test_data = new File([blob], 'fileName')");
    testValue(test_data, function(result) {
        self.result = result;
        compareFiles(result, test_data, callback);
    });
}


function testFileList(callback)
{
    debug("Testing FileList");
    evalAndLog("test_data = document.getElementById('fileInput').files;");
    testValue(test_data, function(result) {
        self.result = result;
        shouldBeTrue("test_data !== result");
        shouldBeEqualToString("Object.prototype.toString.call(result)", "[object FileList]");
        shouldBe("result.length", "test_data.length");
        i = 0;
        function doNext() {
            if (i >= test_data.length) {
                callback();
            } else {
                debug("comparing file[" + i + "]");
                compareFiles(result[i], test_data[i++], doNext);
            }
        }
        doNext();
    });
}

function testArray(callback) 
{
    debug("Testing Array");
    evalAndLog("test_data = []");
    evalAndLog("test_data[0] = 'foo'");
    evalAndLog("test_data[1] = 'bar'");
    evalAndLog("test_data[10] = true");
    evalAndLog("test_data[11] = false");
    evalAndLog("test_data[20] = 123");
    evalAndLog("test_data[21] = 456");
    evalAndLog("test_data[30] = null");

    testValue(test_data, function(result) {
        self.result = result;
        shouldBeTrue("test_data !== result");
        shouldBeTrue("test_data.length === result.length");
        Object.keys(test_data).forEach(
            function(key) {
                shouldBe("test_data[" + key + "]", "result[" + key + "]");
            });
        callback();
    });
}

function testObject(callback) 
{
    debug("Testing Object");
    evalAndLog("test_data = []");
    evalAndLog("test_data[0] = 'foo'");
    evalAndLog("test_data[1] = 'bar'");
    evalAndLog("test_data['a'] = true");
    evalAndLog("test_data['b'] = false");
    evalAndLog("test_data['foo'] = 123");
    evalAndLog("test_data['bar'] = 456");
    evalAndLog("test_data[''] = null");

    testValue(test_data, function(result) {
        self.result = result;
        shouldBeTrue("test_data !== result");
        shouldBeTrue("arrayCompare(Object.keys(result).sort(), Object.keys(test_data).sort())");
        Object.keys(test_data).forEach(
            function(key) {
                shouldBe("test_data[" + JSON.stringify(key) + "]", "result[" + JSON.stringify(key) + "]");
            });
        callback();
    });
}

function testTypedArray(callback) 
{
    debug("Testing TypedArray");
    function testTypedArrayValue(string, callback) {
        evalAndLog("value = " + string);
        test_data = value;
        testValue(test_data, function(result) {
            self.result = result;
            shouldBeTrue("test_data !== result");
            shouldBe("Object.prototype.toString.call(result)", "Object.prototype.toString.call(test_data)");
            shouldBeTrue("test_data.length === result.length");
            for (i = 0; i < test_data.length; ++i) {
                shouldBeTrue("is(test_data[" + i + "], result[" + i + "])");
            }
            callback();
        });
    }

    forEachWithCallback(testTypedArrayValue, [
        "new Uint8Array([])",
        "new Uint8Array([0, 1, 254, 255])",
        "new Uint16Array([0x0000, 0x0001, 0xFFFE, 0xFFFF])",
        "new Uint32Array([0x00000000, 0x00000001, 0xFFFFFFFE, 0xFFFFFFFF])",
        "new Int8Array([0, 1, 254, 255])",
        "new Int16Array([0x0000, 0x0001, 0xFFFE, 0xFFFF])",
        "new Int32Array([0x00000000, 0x00000001, 0xFFFFFFFE, 0xFFFFFFFF])",
        "new Uint8ClampedArray([0, 1, 254, 255])",
        "new Float32Array([-Infinity, -1.5, -1, -0.5, 0, 0.5, 1, 1.5, Infinity, NaN])",
        "new Float64Array([-Infinity, -Number.MAX_VALUE, -Number.MIN_VALUE, 0, Number.MIN_VALUE, Number.MAX_VALUE, Infinity, NaN])"
    ], callback);
}

function testArrays(callback)
{
    debug("Testing Arrays");
    evalAndLog("test_data = []");
    evalAndLog("test_data[0] = []");
    evalAndLog("test_data[1] = [1, 2, 3]");
    evalAndLog("test_data[10] = Object.assign(['foo', 'bar'], {10: true, 11: false, 20: 123, 21: 456, 30: null})");
    evalAndLog("test_data[11] = Object.assign(['foo', 'bar'], {a: true, b: false, foo: 123, bar: 456, '': null})");
    testValue(test_data, function(result) {
        self.result = result;
        shouldBeTrue("test_data !== result");
        shouldBe("typeof test_data", "typeof result");
        shouldBeTrue("test_data.length === result.length");
        Object.keys(test_data).forEach((key) => {
            shouldBeTrue("arrayCompare(test_data[" + key + "], result[" + key + "])");
        });
        callback();
    });
}

function testMap(callback)
{
    debug("Testing Map");
    evalAndLog("test_data = new Map([[1,2],[3,4]])");
    testValue(test_data, function(result) {
        self.result = result;
        shouldBe("typeof test_data", "typeof result");
        shouldBe("test_data.size", "result.size");
        Object.keys(test_data).forEach((key) => {
            shouldBeTrue("test_data[" + key + "] === result[" + key + "]");
        });
        callback();
    });
}

function testSet(callback)
{
    debug("Testing Set");
    evalAndLog("test_data = new Set([1,2,3,4])");
    testValue(test_data, function(result) {
        self.result = result;
        shouldBe("typeof test_data", "typeof result");
        shouldBe("test_data.size", "result.size");
        for (var element of test_data)
            shouldBeTrue("result.has("+ element +")");
        callback();
    });
}

function testGeometryTypes(callback)
{
    debug("Testing geometry types");
    function testOneGeometryType(string, callback) {
        debug("Testing geometry type: " + string);
        var value = eval("value = (" + string + ")");
        test_data = value;
        testValue(test_data, function(result) {
            self.result = result;
            shouldBeTrue("test_data !== result");
            shouldBeEqualToString("typeof result", "object");
            shouldBe("Object.prototype.toString.call(test_data)", "Object.prototype.toString.call(result)");
            shouldBe("test_data.toString()", "result.toString()");
            callback();
        });
    }
    forEachWithCallback(testOneGeometryType, [
        "new DOMMatrix()",
        "new DOMMatrixReadOnly()",
        "new DOMPoint()",
        "new DOMPointReadOnly()",
        "new DOMRect()",
        "new DOMRectReadOnly()",
        "new DOMQuad()"
    ], callback);
}

function testCryptoKey(callback)
{
    debug("Testing CryptoKey");
    evalAndLog("promise = crypto.subtle.generateKey({ name: 'HMAC', hash: {name: 'SHA-512'}}, true, ['sign', 'verify']);");
    promise.then((test_data) => {
        self.test_data = test_data;
        testValue(test_data, function(result) {
            self.result = result;
            shouldBe("typeof test_data", "typeof result");
            shouldBe("test_data.type", "result.type");
            shouldBe("test_data.extractable", "result.extractable");
            shouldBe("test_data.algorithm.toString()", "result.algorithm.toString()");
            shouldBeTrue("arrayCompare(test_data.usages, result.usages)");
            callback();
        });
    });
}

function testBadTypes()
{
    debug("Test types that can't be cloned:");

    evalAndLog("transaction = db.transaction('storeName', 'readwrite')");
    evalAndLog("store = transaction.objectStore('storeName')");
    transaction.onerror = unexpectedErrorCallback;
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = finishJSTest;

    debug("Testing Error");
    evalAndExpectException("store.put(new Error, 'key')", "DOMException.DATA_CLONE_ERR");
    debug("Testing Function");
    evalAndExpectException("store.put(new Function, 'key')", "DOMException.DATA_CLONE_ERR");
    debug("Testing DOMException");
    evalAndExpectException("store.put(new DOMException, 'key')", "DOMException.DATA_CLONE_ERR");

    debug("Testing other host object types");
    evalAndExpectException("store.put(self, 'key')", "DOMException.DATA_CLONE_ERR");
    evalAndExpectException("store.put(document, 'key')", "DOMException.DATA_CLONE_ERR");
    evalAndExpectException("store.put(document.body, 'key')", "DOMException.DATA_CLONE_ERR");
}
