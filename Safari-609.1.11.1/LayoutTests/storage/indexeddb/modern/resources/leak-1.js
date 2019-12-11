description("This tests that certain IDB object relationships don't cause leaks.");

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
var versionChangeObserver;
var dbObserver;
var openRequestObserver;
var objectStoreObserver;

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    versionChangeObserver = internals.observeGC(event.target.transaction);
    dbObserver = internals.observeGC(event.target.result);
    openRequestObserver = internals.observeGC(event.target);

    db = event.target.result;
    objectStoreObserver = internals.observeGC(db.createObjectStore("foo"));

    event.target.onsuccess = function() {
        testGenerator = testSteps();
        testGenerator.next();
    };
}

function* testSteps()
{
    log("Issuing a simple request to the object store");
    
    tx = db.transaction("foo");
    tx.oncomplete = next;
    req = tx.objectStore("foo").get("foo");
    req.onsuccess = next;
    
    yield; // For the request success.
    yield; // For the transaction's completion.
    
    log("Observing GC on the transaction and request. ");
    
    txObserver = internals.observeGC(tx);
    reqObserver = internals.observeGC(req);
    
    tx = null;
    req = null;
    db = null;
    
    // Make sure we are no longer handling any IDB events.
    asyncNext();
    yield;
    
    gc();
    
    shouldBeTrue("txObserver.wasCollected");
    shouldBeTrue("reqObserver.wasCollected");
    shouldBeTrue("versionChangeObserver.wasCollected");
    shouldBeTrue("dbObserver.wasCollected");
    shouldBeTrue("openRequestObserver.wasCollected");
    shouldBeTrue("objectStoreObserver.wasCollected");

    finishJSTest();
 }
 