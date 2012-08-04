description("This tests -[WebScriptObject hasWebScriptKey:(NSString *)key]. It is the equivalent \
    of JavaScript's `in`, to check for the existence of a JavaScript key.");

// Global objects for built-in test functions. This is a key
// we don't expect to be on `window`. Therefore we can add the
// key, and test removing it.
var key = "magic-key";
var object;

function runTestsOnObject(objectParam) {
    object = objectParam;
    objCController.storeWebScriptObject(object);

    // Test for a key that doesn't exist."
    shouldBe("(key in object)", "false");
    shouldBe("objCController.testHasWebScriptKey(key)", "0");

    // Test for a key that exists with a truthy value."
    object[key] = true;
    shouldBe("(key in object)", "true");
    shouldBe("objCController.testHasWebScriptKey(key)", "1");

    // "Test for a key that exists with a falsy value."
    object[key] = false;
    shouldBe("(key in object)", "true");
    shouldBe("objCController.testHasWebScriptKey(key)", "1");

    // Test for a deleted key."
    delete object[key];
    shouldBe("(key in object)", "false");
    shouldBe("objCController.testHasWebScriptKey(key)", "0");

    debug("");
}

if (window.testRunner) {
    testRunner.dumpAsText();

    debug("Test with a newly created, local, object => {}");
    runTestsOnObject({});

    debug("Test with an existing, global, object => window");
    runTestsOnObject(window);

    debug("Test with a DOM Object => document.body");
    runTestsOnObject(document.body);
}

var successfullyParsed = true;
