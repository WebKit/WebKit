description("Test IDBFactory.open's description parameter.");
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function test()
{
    shouldBeTrue("'indexedDB' in window");
    shouldBeFalse("indexedDB == null");

    result = evalAndLog("indexedDB.open('abcd', 'first')");
    verifyResult(result);
    result.onsuccess = firstSuccess;
    result.onerror = unexpectedErrorCallback;
}

function firstSuccess()
{
    verifySuccessEvent(event);
    window.firstDB = event.result;

    shouldBeEqualToString('event.result.description', 'first');

    result = evalAndLog("indexedDB.open('abcd', 'second')");
    verifyResult(result);
    result.onsuccess = secondSuccess;
    result.onerror = unexpectedErrorCallback;
}

function secondSuccess()
{
    verifySuccessEvent(event);
    window.secondDB = event.result;

    shouldBeEqualToString('firstDB.description', 'first');
    shouldBeEqualToString('secondDB.description', 'second');

    result = evalAndLog("indexedDB.open('abcd')");
    verifyResult(result);
    result.onsuccess = thirdSuccess;
    result.onerror = unexpectedErrorCallback;
}

function thirdSuccess()
{
    verifySuccessEvent(event);

    shouldBeEqualToString('firstDB.description', 'first');
    shouldBeEqualToString('secondDB.description', 'second');
    shouldBeEqualToString('event.result.description', 'second');

    done();
}

test();

var successfullyParsed = true;
