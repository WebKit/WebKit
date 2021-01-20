description("This tests expectations with renaming existing indexes.");

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

	objectStore = db.createObjectStore("ObjectStore");

	objectStore.createIndex("ExistingIndex", "foo");
	
	indexToDelete = objectStore.createIndex("IndexToDelete", "bar");
	objectStore.deleteIndex("IndexToDelete");
	try {
		indexToDelete.name = "RenamedIndex";
		log("Was incorrectly able to rename a deleted index");
	} catch (e) {
		log("Failed to rename deleted index: " + e)
	}
	
	newIndex = objectStore.createIndex("NewIndex", "bar");
	
	log("Initial index name: " + newIndex.name);
	
	try {
		newIndex.name = "ExistingIndex";
		log("Was incorrectly able to rename this index to the name of another existing index");
	} catch(e) {
		log("Caught exception renaming index to the name of another existing index: " + e);
	}

	newIndex.name = newIndex.name;
	log("Renamed this index to the same name it already has, it's name is now: " + newIndex.name);

	newIndex.name = "RenamedIndex";
	log("Renamed index name: " + newIndex.name);

    event.target.onsuccess = function() {
        testGenerator = testSteps();
        testGenerator.next();
    };
}

function* testSteps()
{    
    tx = db.transaction("ObjectStore", "readwrite");
    tx.oncomplete = next;
    objectStore = tx.objectStore("ObjectStore");
	index = objectStore.index("RenamedIndex");
    log("Current index name in a new transaction: " + index.name);

	try {
		index.name = "newName";
		log("Was incorrectly able to rename this index outside of a version change transaction");
	} catch(e) {
		log("Caught exception renaming index outside of a version change transaction: " + e);
	}
    
    yield; // For the transaction's completion.

	db.close();
    
	upgradeOpenRequest = indexedDB.open(dbName, dbVersion + 1);
	upgradeOpenRequest.onerror = next;
	upgradeOpenRequest.onupgradeneeded = function(event) {
		objectStore = event.target.transaction.objectStore("ObjectStore");
		
		log("Current indexNames during second upgrade transaction includes 'ExistingIndex': " + objectStore.indexNames.contains("ExistingIndex"));
		log("Current indexNames during second upgrade transaction includes 'RenamedIndex': " + objectStore.indexNames.contains("RenamedIndex"));
		log("Current indexNames during second upgrade transaction includes 'AnotherName': " + objectStore.indexNames.contains("AnotherName"));
		log("Current indexNames during second upgrade transaction includes 'YetAnotherName': " + objectStore.indexNames.contains("YetAnotherName"));

		index = objectStore.index("RenamedIndex");
		index.name = "AnotherName";
		log("Renamed index to: " + index.name);
		
		log("Current indexNames during second upgrade transaction includes 'ExistingIndex': " + objectStore.indexNames.contains("ExistingIndex"));
		log("Current indexNames during second upgrade transaction includes 'RenamedIndex': " + objectStore.indexNames.contains("RenamedIndex"));
		log("Current indexNames during second upgrade transaction includes 'AnotherName': " + objectStore.indexNames.contains("AnotherName"));
		log("Current indexNames during second upgrade transaction includes 'YetAnotherName': " + objectStore.indexNames.contains("YetAnotherName"));

		index.name = "YetAnotherName";
		log("Renamed index again to: " + index.name);

		log("Current indexNames during second upgrade transaction includes 'ExistingIndex': " + objectStore.indexNames.contains("ExistingIndex"));
		log("Current indexNames during second upgrade transaction includes 'RenamedIndex': " + objectStore.indexNames.contains("RenamedIndex"));
		log("Current indexNames during second upgrade transaction includes 'AnotherName': " + objectStore.indexNames.contains("AnotherName"));
		log("Current indexNames during second upgrade transaction includes 'YetAnotherName': " + objectStore.indexNames.contains("YetAnotherName"));

		log("Aborting version change transaction...");
		event.target.transaction.abort();
	}

	yield; // For the open request's failure (due to aborting the version change transaction)

	upgradeOpenRequest = indexedDB.open(dbName);
	upgradeOpenRequest.onsuccess = function(event) {
		objectStore = event.target.result.transaction("ObjectStore").objectStore("ObjectStore");
		
		log("Current indexNames during second upgrade transaction includes 'ExistingIndex': " + objectStore.indexNames.contains("ExistingIndex"));
		log("Current indexNames during second upgrade transaction includes 'RenamedIndex': " + objectStore.indexNames.contains("RenamedIndex"));
		log("Current indexNames during second upgrade transaction includes 'AnotherName': " + objectStore.indexNames.contains("AnotherName"));
		log("Current indexNames during second upgrade transaction includes 'YetAnotherName': " + objectStore.indexNames.contains("YetAnotherName"));

		next();
	}
	
	yield; // To balance the next() above.
	
    finishJSTest();
 }
 