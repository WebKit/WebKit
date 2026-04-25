description("This tests expectations with renaming existing object stores.");

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

	objectStore = db.createObjectStore("ExistingObjectStore");

	objectStore = db.createObjectStore("TestObjectStore");
	log("Initial objectStore name: " + objectStore.name);
	
	try {
		objectStore.name = "ExistingObjectStore";
		log("Was incorrectly able to rename this object store to the name of another existing object store");
	} catch(e) {
		log("Caught exception renaming object store to the name of another existing object store: " + e);
	}

	objectStore.name = "RenamedObjectStore";
	log("Renamed objectStore name: " + objectStore.name);

    event.target.onsuccess = function() {
        testGenerator = testSteps();
        testGenerator.next();
    };
}

function* testSteps()
{    
    tx = db.transaction("RenamedObjectStore", "readwrite");
    tx.oncomplete = next;
    objectStore = tx.objectStore("RenamedObjectStore");
    log("Current objectStore name in a new transaction: " + objectStore.name);

	try {
		objectStore.name = "newName";
		log("Renaming object store succeeded, but it shouldn't have");
	} catch(e) {
		log("Caught exception renaming object store outside of a version change transaction: " + e);
	}
    
    yield; // For the transaction's completion.

	db.close();
    
	upgradeOpenRequest = indexedDB.open(dbName, dbVersion + 1);
	upgradeOpenRequest.onerror = next;
	upgradeOpenRequest.onupgradeneeded = function() {
		db = upgradeOpenRequest.result;
		
		log("Current objectStoreNames during second upgrade transaction includes 'ExistingObjectStore': " + db.objectStoreNames.contains("ExistingObjectStore"));
		log("Current objectStoreNames during second upgrade transaction includes 'RenamedObjectStore': " + db.objectStoreNames.contains("RenamedObjectStore"));

		objectStore = event.target.transaction.objectStore("RenamedObjectStore");
		objectStore.name = "YetAnotherName";
		
		log("Renamed objectstore again to: " + objectStore.name);
		log("Aborting version change transaction...");
		event.target.transaction.abort();
	}

	yield; // For the open request's failure (due to aborting the version change transaction)

	upgradeOpenRequest = indexedDB.open(dbName);
	upgradeOpenRequest.onsuccess = function(event) {
		log("Current objectStoreNames during final transaction includes 'ExistingObjectStore': " + db.objectStoreNames.contains("ExistingObjectStore"));
		log("Current objectStoreNames during final transaction includes 'RenamedObjectStore': " + db.objectStoreNames.contains("RenamedObjectStore"));

		next();
	}
	
	yield; // To balance the next() above.
	
    finishJSTest();
 }
 