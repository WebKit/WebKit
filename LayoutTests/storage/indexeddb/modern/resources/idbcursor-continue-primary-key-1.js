// Test modeled after imported/w3c/web-platform-tests/IndexedDB/idbcursor-continuePrimaryKey-exception-order.htm

description("This test verifies the basic functionality of IDBCursor.continuePrimaryKey().");

function setup_test_store(db) {
	var records = [ { iKey: "A", pKey: 1 },
	                { iKey: "A", pKey: 2 },
	                { iKey: "A", pKey: 3 },
	                { iKey: "A", pKey: 4 },
	                { iKey: "B", pKey: 5 },
	                { iKey: "B", pKey: 6 },
	                { iKey: "B", pKey: 7 },
	                { iKey: "C", pKey: 8 },
	                { iKey: "C", pKey: 9 },
	                { iKey: "D", pKey: 10 } ];

    var store = db.createObjectStore("test", { keyPath: "pKey" });
    var index = store.createIndex("idx", "iKey");

    for(var i = 0; i < records.length; i++)
        store.add(records[i]);

    return store;
}

function log(message)
{
    debug(message);
}

function next()
{
    testGenerator.next();
}

function asyncNext()
{
    setTimeout("testGenerator.next();", 0);
}

function unreached(description)
{
	return function() {
		debug("FAIL: " + description);
		finishJSTest();
	}
};

var dbName = window.location.href + " test";
var db;

var testGenerator = testSteps();
testGenerator.next();

function shouldThrowType(name, func, text) {
    var errorText = text + " should throw an exception";
    try {
        func();
        testFailed(errorText);
    } catch (e) {
		if (e.name == name)
        	testPassed('"' + text + "\" threw as expected");
        else
			testFailed(text + " should throw exception type " + name + ", but threw " + e.name + " instead");
    }
}

var db;
function* testSteps()
{
	indexedDB.deleteDatabase(dbName).onsuccess = next;
	yield;
	
	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;
		var txn = request.transaction;

		var store = setup_test_store(db);
		var index = store.index("idx");
		var cursor_rq = index.openCursor();

		cursor_rq.onerror = unreached('openCursor should succeed');
		cursor_rq.onsuccess = function(e) {
		    cursor = e.target.result;
            shouldBeTrue("!!cursor");

		    store.deleteIndex("idx");
		};
		txn.oncomplete = function() {
		    shouldThrowType("TransactionInactiveError", function() {
		        cursor.continuePrimaryKey("A", 4);
		    }, "transaction-state check should precede deletion check");
			db.close();
			next();
		};
	};
	yield;

	cursor = undefined;
	indexedDB.deleteDatabase(dbName).onsuccess = next;
	yield;

	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;

		var store = setup_test_store(db);
        var cursor_rq = store.openCursor();

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            cursor = e.target.result;
            shouldBeTrue("!!cursor");

            db.deleteObjectStore("test");

            shouldThrowType("InvalidStateError", function() {
                cursor.continuePrimaryKey("A", 4);
            }, "deletion check should precede index source check");

			db.close();
            next();
        };
	};
	yield;

	cursor = undefined;
	indexedDB.deleteDatabase(dbName).onsuccess = next;
	yield;

	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;
		
        var store = setup_test_store(db);
        var index = store.index("idx");
        var cursor_rq = index.openCursor(null, "nextunique");

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            cursor = e.target.result;
            shouldBeTrue("!!cursor");

            store.deleteIndex("idx");

            shouldThrowType("InvalidStateError", function() {
              cursor.continuePrimaryKey("A", 4);
            }, "deletion check should precede cursor direction check");
			
			db.close();
            next();
        };
	};
	yield;

	cursor = undefined;
	indexedDB.deleteDatabase(dbName).onsuccess = next;
	yield;

	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;
		
        var store = db.createObjectStore("test", {keyPath:"pKey"});
        var index = store.createIndex("idx", "iKey");

        store.add({ iKey: "A", pKey: 1 });

        var cursor_rq = index.openCursor(null, "nextunique");

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            if (e.target.result) {
                cursor = e.target.result;
                cursor.continue();
                return;
            }

            shouldThrowType("InvalidAccessError", function() {
                cursor.continuePrimaryKey("A", 4);
            }, "direction check should precede got_value_flag check");

			db.close();
            next();
        };
	};
	yield;

	indexedDB.deleteDatabase(dbName).onsuccess = next;
	cursor = undefined;
	yield;
	
	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;
	
        var store = db.createObjectStore("test", {keyPath:"pKey"});
        var index = store.createIndex("idx", "iKey");

        store.add({ iKey: "A", pKey: 1 });

        var cursor_rq = index.openCursor(null, "nextunique");

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            if (!cursor) {
                cursor = e.target.result;
                shouldBeTrue("!!cursor");

                cursor.continue();

                shouldThrowType("InvalidAccessError", function() {
                    cursor.continuePrimaryKey("A", 4);
                }, "direction check should precede iteration ongoing check");

				cursor_rq.onsuccess = undefined;
				db.close();
                next();
            }
        };
	};
	yield;
	
	indexedDB.deleteDatabase(dbName).onsuccess = next;
	cursor = undefined;
	yield;
	
	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;
		
        var store = setup_test_store(db);
        var cursor_rq = store.openCursor();

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            if (!cursor) {
                cursor = e.target.result;
                shouldBeTrue("!!cursor");

                cursor.continue();

                shouldThrowType("InvalidAccessError", function() {
                    cursor.continuePrimaryKey("A", 4);
                }, "index source check should precede iteration ongoing check");

				cursor_rq.onsuccess = undefined;
		        db.close();
                next();
            }
       	};
	};
	yield;

	indexedDB.deleteDatabase(dbName).onsuccess = next;
	cursor = undefined;
	yield;
	
	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;
		
        var store = db.createObjectStore("test", {keyPath:"pKey"});

        store.add({ iKey: "A", pKey: 1 });

        var cursor_rq = store.openCursor();

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            if (e.target.result) {
                cursor = e.target.result;
                cursor.continue();
                return;
            }

            shouldThrowType("InvalidAccessError", function() {
                cursor.continuePrimaryKey("A", 4);
            }, "index source check should precede got_value_flag check");
		    db.close();
            next();
        };
	};
	yield;

	indexedDB.deleteDatabase(dbName).onsuccess = next;
	cursor = undefined;
	yield;
	
	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;
		
        var store = setup_test_store(db);
        var index = store.index("idx");
        var cursor_rq = index.openCursor();

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            if (!cursor) {
                cursor = e.target.result;
                shouldBeTrue("!!cursor");

                cursor.continue();

                shouldThrowType("InvalidStateError", function() {
                    cursor.continuePrimaryKey(null, 4);
                }, "iteration ongoing check should precede unset key check");

				cursor_rq.onsuccess = undefined;
		    	db.close();
                next();
            }
        };
	};
	yield;

	indexedDB.deleteDatabase(dbName).onsuccess = next;
	cursor = undefined;
	yield;
	
	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;
		
        var store = db.createObjectStore("test", {keyPath:"pKey"});
        var index = store.createIndex("idx", "iKey");

        store.add({ iKey: "A", pKey: 1 });

        var cursor_rq = index.openCursor();

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            if (e.target.result) {
                cursor = e.target.result;
                cursor.continue();
                return;
            }

            shouldThrowType("InvalidStateError", function() {
                cursor.continuePrimaryKey(null, 4);
            }, "got_value_flag check should precede unset key check");
	   		
			db.close();
            next();
        };
	};
	yield;

	indexedDB.deleteDatabase(dbName).onsuccess = next;
	cursor = undefined;
	yield;
	
	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;

        var store = setup_test_store(db);
        var index = store.index("idx");
        var cursor_rq = index.openCursor();

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            cursor = e.target.result;
            shouldBeTrue("!!cursor");

            shouldThrowType("DataError", function() {
                cursor.continuePrimaryKey(null, 4);
            }, "DataError is expected if key is unset.");

			db.close();
            next();
        };
	};
	yield;

	indexedDB.deleteDatabase(dbName).onsuccess = next;
	cursor = undefined;
	yield;
	
	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;

        var store = setup_test_store(db);
        var index = store.index("idx");
        var cursor_rq = index.openCursor();

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            cursor = e.target.result;
            shouldBeTrue("!!cursor");

            shouldThrowType("DataError", function() {
                cursor.continuePrimaryKey("A", null);
            }, "DataError is expected if primary key is unset.");

			db.close();
            next();
        };
	};
	yield;
	
	indexedDB.deleteDatabase(dbName).onsuccess = next;
	cursor = undefined;
	yield;
	
	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;

        var store = setup_test_store(db);
        var index = store.index("idx");
        var cursor_rq = index.openCursor(IDBKeyRange.lowerBound("B"));

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            cursor = e.target.result;
            shouldBeTrue("!!cursor");

            shouldBeTrue("cursor.key == 'B'", "expected key");
            shouldBeTrue("cursor.primaryKey == 5", "expected primary key");

            shouldThrowType("DataError", function() {
                cursor.continuePrimaryKey("A", 6);
            }, "DataError is expected if key is lower then current one.");

            shouldThrowType("DataError", function() {
                cursor.continuePrimaryKey("B", 5);
            }, "DataError is expected if primary key is equal to current one.");

            shouldThrowType("DataError", function() {
                cursor.continuePrimaryKey("B", 4);
            }, "DataError is expected if primary key is lower than current one.");

			db.close();
            next();
        };
	};
	yield;

	indexedDB.deleteDatabase(dbName).onsuccess = next;
	cursor = undefined;
	yield;
	
	request = indexedDB.open(dbName);
	request.onupgradeneeded = function() {
		db = request.result;
	
        var store = setup_test_store(db);
        var index = store.index("idx");
        var cursor_rq = index.openCursor(IDBKeyRange.upperBound("B"), "prev");

        cursor_rq.onerror = unreached('openCursor should succeed');
        cursor_rq.onsuccess = function(e) {
            cursor = e.target.result;
            shouldBeTrue("!!cursor");

            shouldBeTrue("cursor.key == 'B'", "expected key");
            shouldBeTrue("cursor.primaryKey == 7", "expected primary key");

            shouldThrowType("DataError", function() {
                cursor.continuePrimaryKey("C", 6);
            }, "DataError is expected if key is larger then current one.");

            shouldThrowType("DataError", function() {
                cursor.continuePrimaryKey("B", 7);
            }, "DataError is expected if primary key is equal to current one.");

            shouldThrowType("DataError", function() {
                cursor.continuePrimaryKey("B", 8);
            }, "DataError is expected if primary key is larger than current one.");

			db.close();
            next();
        };
	};
	yield;

	finishJSTest();
}
