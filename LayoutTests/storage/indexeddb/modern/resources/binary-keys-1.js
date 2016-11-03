description("This test verifies the basic use of binary keys.");

indexedDBTest(prepareDatabase);

function log(message)
{
    debug(message);
}

var testGenerator;
function next()
{
    testGenerator.next();
}

function asyncNext()
{
    setTimeout("testGenerator.next();", 0);
}

var db;
var dbName;
var dbVersion;

function prepareDatabase(event)
{
    log("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    db = event.target.result;
    dbName = db.name;
    dbVersion = db.version;

    db.createObjectStore("TestObjectStore");
    
    event.target.onsuccess = function() {
        testGenerator = testSteps();
        testGenerator.next();
    };
}

// Some testing values borrowed from <root>/LayoutTests/imported/w3c/web-platform-tests/IndexedDB/idb_binary_key_conversion.htm
function* testSteps()
{
    binary = new ArrayBuffer(0);
    key = IDBKeyRange.lowerBound(binary).lower;

    shouldBeTrue("key instanceof ArrayBuffer");
    shouldBe("key.byteLength", "0");
    shouldBe("key.byteLength", "binary.byteLength");

    // Key based on ArrayBuffer
    binary = new ArrayBuffer(4);
    dataView = new DataView(binary);
    dataView.setUint32(0, 1234567890);
    key = IDBKeyRange.lowerBound(binary).lower;

    shouldBeTrue("key instanceof ArrayBuffer");
    shouldBe("key.byteLength", "4");
    shouldBe("dataView.getUint32(0)", "new DataView(key).getUint32(0)");

    // Key based on DataView
    binary = new ArrayBuffer(4);
    dataView = new DataView(binary);
    dataView.setUint32(0, 1234567890);
    key = IDBKeyRange.lowerBound(dataView).lower;

    shouldBeTrue("key instanceof ArrayBuffer");
    shouldBe("key.byteLength", "4");
    shouldBe("dataView.getUint32(0)", "new DataView(key).getUint32(0)");

    // Typed array
    binary = new ArrayBuffer(4);
    dataView = new DataView(binary);
    int8Array = new Int8Array(binary);
    int8Array.set([16, -32, 64, -128]);

    key = IDBKeyRange.lowerBound(int8Array).lower;
    keyInInt8Array = new Int8Array(key);

    shouldBeTrue("key instanceof ArrayBuffer");
    shouldBe("key.byteLength", "4");
    for (i = 0; i < int8Array.length; ++i) {
        shouldBe("keyInInt8Array[i]", "int8Array[i]");
    }

    transaction = db.transaction("TestObjectStore", "readwrite");
    objectStore = transaction.objectStore("TestObjectStore");
    objectStore.put("Value", dataView).onsuccess = next;
    yield;

	objectStore.openCursor().onsuccess = function(event) {
		debug("Got the key and value with a cursor");
		key = event.target.result.key;
	    keyInInt8Array = new Int8Array(key);

    	shouldBeTrue("key instanceof ArrayBuffer");
    	shouldBe("key.byteLength", "4");
    	for (i = 0; i < int8Array.length; ++i) {
        	shouldBe("keyInInt8Array[i]", "int8Array[i]");
    	}

		retrievedValue = event.target.result.value;
		expectedValue = "Value"
		shouldBe("retrievedValue", "expectedValue");
		
		next();
	}
	yield;

    objectStore.get(dataView).onsuccess = function(event) {
        debug("Got the value with a DataView key: " + event.target.result);
        next();
    }
    yield;

    objectStore.get(binary).onsuccess = function(event) {
        debug("Got the value with an ArrayBuffer key: " + event.target.result);
        next();
    }
    yield;

    objectStore.get(int8Array).onsuccess = function(event) {
        debug("Got the value with a Typed Array key: " + event.target.result);
        next();
    }
    yield;
    
    int8Array.set([10, 10, 10, 10]);
    objectStore.get(int8Array).onsuccess = function(event) {
        debug("Got the value with a Typed Array key, but changed from the original put: " + event.target.result);
        next();
    }
    yield;

    finishJSTest();
}

