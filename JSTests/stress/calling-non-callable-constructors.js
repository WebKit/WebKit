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

shouldThrow(() => {
    class C extends Object {
      constructor(t = super()) {}
    }

    C();
}, `TypeError: Cannot call a class constructor without |new|`);

shouldThrow(() => {
    Promise();
}, `TypeError: Cannot call a constructor without |new|`);

shouldThrow(() => {
    class DerivedPromise extends Promise { }
    DerivedPromise();
}, `TypeError: Cannot call a class constructor without |new|`);
