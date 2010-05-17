description("Test the basics of IndexedDB.");

if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}


function eventShared()
{
    debug("");
    shouldBeTrue("'source' in event");
    shouldBeTrue("'open' in event.source");
    debug("");
    shouldBeTrue("'result' in event");
    shouldBeFalse("'code' in event");
    shouldBeFalse("'message' in event");
    debug("");
    shouldBeTrue("'onsuccess' in event.target");
    shouldBeTrue("'onerror' in event.target");
    shouldBeTrue("'abort' in event.target");
    shouldBeTrue("'readyState' in event.target");
    shouldBe("event.target.readyState", "event.target.DONE");
    debug("");
    window.successfullyParsed = true;
    isSuccessfullyParsed();
    if (window.layoutTestController)
            layoutTestController.notifyDone()
}

function successFunction()
{
    debug("Success function called");
    eventShared();
}

function errorFunction()
{
    testFailed("Error function called: (" + event.code + ") " + event.message);
    eventShared();
}

function test()
{
    shouldBeTrue("'indexedDB' in window");
    shouldBeTrue("'open' in indexedDB");

    result = evalAndLog("indexedDB.open('name', 'description', true /* allow modification */)");
    shouldBeTrue("'onsuccess' in result");
    shouldBeTrue("'onerror' in result");
    shouldBeTrue("'abort' in result");
    shouldBeTrue("'readyState' in result");
    result.onsuccess = successFunction;
    result.onerror = errorFunction;
    debug("");
    debug("An event should fire shortly...");
    debug("");
}

test();

