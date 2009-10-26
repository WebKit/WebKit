description("Test that changing documentURI has no effects on storage events.");

function test(storageString)
{
    window.storage = eval(storageString);
    if (!storage) {
        testFailed(storageString + " DOES NOT exist");
        return;
    }

    debug("Testing " + storageString);

    window.onstorage = null;
    evalAndLog("storage.clear()");
    shouldBe("storage.length", "0");

    evalAndLog("window.onstorage = firstEvent");
    evalAndLog("storage.foo = '123'");
}

function firstEvent(e)
{
    debug("First event fired");
    debug("Saving URI");
    window.lastURI = e.uri;

    evalAndLog("document.documentURI = 'abc'");
    shouldBeEqualToString("document.documentURI", "abc");

    evalAndLog("window.onstorage = secondEvent");
    evalAndLog("storage.foo = 'xyz'");
}

function secondEvent(e)
{
    debug("Second event fired");
    shouldBeTrue(String(window.lastURI == e.uri));

    if (done) {
        window.successfullyParsed = true;
        isSuccessfullyParsed();
    }
}

window.done = false;
test("sessionStorage");
debug("");
debug("");
window.done = true;
test("localStorage");
