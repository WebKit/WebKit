//@ requireOptions("--useShadowRealm=1")

var abort = $vm.abort;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}! got ${error.name}`);
}

async function shouldThrowAsync(func, errorType) {
    let error;
    try {
        await func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}! got ${error.name} with message ${error.message}`);
}

(async function () {
    const importPath = "./resources/shadow-realm-example-module.js";
    const { answer, getCallCount, putInGlobal, getFromGlobal, getAnObject } = await import(importPath);
    const outerAnswer = answer;
    const outerGetCallCount = getCallCount;
    const outerPutInGlobal = putInGlobal;
    const outerGetFromGlobal = getFromGlobal;
    const outerGetAnObject = getAnObject;

    {
        let realm = new ShadowRealm();

        // update local module state + check it
        shouldBe(outerGetCallCount(), 0);
        outerGetAnObject();
        shouldBe(outerGetCallCount(), 1);
        // update realm module state + check it
        let innerGetCallCount = await realm.importValue(importPath, "getCallCount");
        let innerPutInGlobal = await realm.importValue(importPath, "putInGlobal");
        shouldBe(innerGetCallCount(), 0);
        innerPutInGlobal("something", "random");
        shouldBe(innerGetCallCount(), 1);
        // re-importing the module into the realm doesn't reload the module
        innerGetCallCount = await realm.importValue(importPath, "getCallCount");
        shouldBe(innerGetCallCount(), 1);
        // the (outer) local module state stays intact
        shouldBe(outerGetCallCount(), 1);

        // one can imported primitive/callable variables just fine
        // shouldBe(innerAnswer, outerAnswer);
        let innerAnswer = await realm.importValue(importPath, "answer");
        shouldBe(innerAnswer, outerAnswer);

        // imported variables are checked for primtive/callable-ness
        await shouldThrowAsync(async () => { let x = await realm.importValue(importPath, "anObject"); }, TypeError);

        // importing non-existent ref fails
        await shouldThrowAsync(async () => { let x = await realm.importValue(importPath, "nothing"); }, TypeError);

        // importing from non-existent file fails
        await shouldThrowAsync(async () => { let x = await realm.importValue("random", "nothing"); }, TypeError);

        // we can import functions through an inner realm for use in the outer
        let innerGetFromGlobal = await realm.importValue(importPath, "getFromGlobal");
        innerPutInGlobal("salutation", "sarava");
        shouldBe(innerGetFromGlobal("salutation"), "sarava");

        // inner global state is unchanged by outer global state change
        outerPutInGlobal("salutation", "hello world!");
        shouldBe(outerGetFromGlobal("salutation"), "hello world!");
        shouldBe(innerGetFromGlobal("salutation"), "sarava");

        // wrapped functions check arguments for primitive/callable-ness
        shouldThrow(() => { innerPutInGlobal("treasure", new Object()); }, TypeError);

        // imported functions are wrapped with correct return value checks
        let getAnObjectFn = await realm.importValue(importPath, "getAnObject");
        shouldThrow(() => { getAnObjectFn(); }, TypeError);

        // thread a function in and out of a realm to wrap it up
        innerPutInGlobal("outer-realm-put", outerPutInGlobal);
        wrappedOuterPutInGlobal = innerGetFromGlobal("outer-realm-put");

        // it still manipuates the correct global object state
        wrappedOuterPutInGlobal("treasure", "shiny tin scrap");
        shouldBe(outerGetFromGlobal("treasure"), "shiny tin scrap");

        // wrapped functions check arguments for primitive/callable-ness
        shouldThrow(() => { wrappedOuterPutInGlobal("treasure", new Object()); }, TypeError);
        shouldThrow(() => { wrappedOuterPutInGlobal(new Object(), "shiny tin scrap"); }, TypeError);

        // must be called on a ShadowRealm
        let notRealm = {};
        shouldThrow(
          () => { realm.importValue.call(notRealm, importPath, "answer"); },
          TypeError,
          (err) => { shouldBe($.globalObjectFor(err), globalThis); }
        );
    }

    // trigger JIT
    {
        function doImport(realm, s)
        {
            return realm.importValue(importPath, s);
        }

        noInline(doImport);

        let realm = new ShadowRealm();
        for (var i = 0; i < 10000; ++i) {
            let result = await doImport(realm, "getCallCount");
            shouldBe(result(), 0);
        }
    }
}()).catch((error) => {
    print(String(error));
    abort();
});

{
    shouldBe(typeof ShadowRealm.prototype.importValue, "function");

    let importValueName = Object.getOwnPropertyDescriptor(ShadowRealm.prototype.importValue, "name");
    shouldBe(importValueName.value, "importValue");
    shouldBe(importValueName.enumerable, false);
    shouldBe(importValueName.writable, false);
    shouldBe(importValueName.configurable, true);

    let importValueLength = Object.getOwnPropertyDescriptor(ShadowRealm.prototype.importValue, "length");
    shouldBe(importValueLength.value, 2);
    shouldBe(importValueLength.enumerable, false);
    shouldBe(importValueLength.writable, false);
    shouldBe(importValueLength.configurable, true);
}
