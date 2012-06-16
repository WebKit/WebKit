description("This test checks to ensure that window.localStorage and window.sessionStorage are not rendered unusable by setting a key with the same name as a storage function such that the function is hidden.");

if (window.testRunner)
    testRunner.dumpAsText();

function doWedgeThySelf(storage) {
    storage.setItem("clear", "almost");
    storage.setItem("key", "too");
    storage.setItem("getItem", "funny");
    storage.setItem("removeItem", "to");
    storage.setItem("length", "be");
    storage.setItem("setItem", "true");
}

function testStorage(storageString) {
    storage = eval(storageString);
    storage.clear();
    doWedgeThySelf(storage);
    shouldBeEqualToString("storage.getItem('clear')", "almost");
    shouldBeEqualToString("storage.getItem('key')", "too");
    shouldBeEqualToString("storage.getItem('getItem')", "funny");
    shouldBeEqualToString("storage.getItem('removeItem')", "to");
    shouldBeEqualToString("storage.getItem('length')", "be");
    shouldBeEqualToString("storage.getItem('setItem')", "true");
        
    // Test to see if an exception is thrown for any of the built in
    // functions.
    storage.setItem("test", "123");
    storage.key(0);
    storage.getItem("test");
    storage.removeItem("test");
    storage.clear();
    if (storage.length != 0)
        throw name + ": length wedged";
}


function runTest()
{
    testStorage(window.sessionStorage);
    testStorage(window.localStorage);
}

try {
    runTest();
} catch (e) {
    testFailed("Caught an exception!");
}

