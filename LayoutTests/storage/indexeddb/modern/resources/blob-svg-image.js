description("This tests that retrieving blobs from IDB successfully registers their content type");

indexedDBTest(prepareDatabase);

var testGenerator;
var svgData = ["<svg xmlns='http://www.w3.org/2000/svg'><circle r='200' cx='200' cy='200' stroke='red' stroke-width='1' fill='yellow' /></svg>"];

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
}

var db;

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    debug(event.target.result.name);
    db = event.target.result;
    db.createObjectStore("TestObjectStore");
    event.target.onsuccess = function() {
        testGenerator = testSteps();
        testGenerator.next();
    };
}

function* testSteps()
{
    debug("Let's create an svg blob and store it in IndexedDB.");

    blob = new Blob(svgData, { type: "image/svg+xml" });

    var transaction = db.transaction("TestObjectStore", "readwrite");
    transaction.oncomplete = continueWithEvent;
    
    idbRequest(transaction.objectStore("TestObjectStore").add(blob, "foo"));
    event = yield;
    debug("Added blob to database");

    event = yield;
    debug("Transaction complete. Now let's navigate the original window to continue the test");

    blob = null;

    window.opener.location.href = "blob-svg-image2.html";
}