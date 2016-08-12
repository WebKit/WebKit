function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: {String(actual)}`);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var executorCalled = false;
shouldThrow(() => {
    Promise(function (resolve, reject) { executorCalled = true; });
}, `TypeError: calling Promise constructor without new is invalid`);
shouldBe(executorCalled, false);

// But should accept inheriting Promise.
class Deferred extends Promise {
    constructor()
    {
        let resolve, reject;
        super(function (aResolve, aReject) {
            this.resolve = aResolve;
            this.reject = aReject;
        });
    }
}

{
    let deferred = new Deferred();
    shouldBe(deferred.__proto__, Deferred.prototype);
    shouldBe(deferred.__proto__.__proto__, Promise.prototype);
    shouldBe(Deferred.__proto__, Promise);
}
