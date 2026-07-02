function shouldThrow(func, errorMessage) {
    var errorThrown = false;
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

class MyFunction extends Function {
    constructor() {
        super();

        super.prototype = 1;
    }
}

function test1() {
    const f = new MyFunction();
    f.__defineGetter__("prototype", () => {}); // should throw
}

function test2(i) {
    const f = new MyFunction();
    try { f.__defineGetter__("prototype", () => {}); } catch {}
    f.prototype.x = i; // should not crash
}

(function() {
    for (var i = 0; i < 1e4; i++)
        shouldThrow(test1, "TypeError: Attempting to change configurable attribute of unconfigurable property.");
})();

(function() {
    for (var i = 0; i < 1e4; i++)
        test2();
})();
