function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== errorMessage)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

(function stringLength() {
    let str = "foo";

    for (let i = 0; i < 1e5; i++) {
        str.length = 0;
        shouldBe(str.length, 3);
        shouldThrow(() => { "use strict"; "bar".length = 3; }, "TypeError: Attempted to assign to readonly property.");
    }
})();
