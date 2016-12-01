description("Test IDBObjectStore.getKey()");

indexedDBTest(prepareDatabase);

function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var testGenerator;

function continueWithEvent(event)
{
    testGenerator.next(event);
}

function asyncContinue()
{
    setTimeout("testGenerator.next();", 0);
}

function idbRequest(request)
{
    request.onerror = continueWithEvent;
    request.onsuccess = continueWithEvent;
	return request;
}

var db;

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    db = event.target.result;
    os = db.createObjectStore("foo");
	os.add(false, 1);
	os.add(-10, 2);
	os.add(10, 3);
	os.add("hello", 4);
	os.add("hellothere", 5);

    event.target.transaction.oncomplete = function() {
        testGenerator = testSteps();
        testGenerator.next();
    };
}

function* testSteps()
{
    objectStore = db.transaction("foo").objectStore("foo");
	
	// Non-existent key
    req = idbRequest(objectStore.getKey(6));
    event = yield;    
	debug("getKey(6) result is: " + req.result);

	// Existent key
    req = idbRequest(objectStore.getKey(3));
    event = yield;    
	debug("getKey(3) result is: " + req.result);
	
	// Key range only
    req = idbRequest(objectStore.getKey(IDBKeyRange.only(5)));
    event = yield;    
	debug("getKey(IDBKeyRange.only(5)) result is: " + req.result);

	// Key range lower bound
    req = idbRequest(objectStore.getKey(IDBKeyRange.lowerBound(2)));
    event = yield;    
	debug("getKey(IDBKeyRange.lowerBound(2)) result is: " + req.result);

	// Key range upper bound
    req = idbRequest(objectStore.getKey(IDBKeyRange.upperBound(2)));
    event = yield;    
	debug("getKey(IDBKeyRange.upperBound(2)) result is: " + req.result);

	// Key range bound
    req = idbRequest(objectStore.getKey(IDBKeyRange.bound(2, 4)));
    event = yield;    
	debug("getKey(IDBKeyRange.bound(2, 4)) result is: " + req.result);
    
    finishJSTest();
 }
 