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

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    db = event.target.result;
    db.createObjectStore("foo");
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
    
    // Make sure we are no longer handling any IDB events.
    asyncNext();
    yield;
    
    gc();
    
    shouldBeTrue("txObserver.wasCollected");
    shouldBeTrue("reqObserver.wasCollected");

    finishJSTest();
 }
 