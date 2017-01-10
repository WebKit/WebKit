function shouldBe(expected, actual, msg) {
    if (msg === void 0)
        msg = "";
    else
        msg = " for " + msg;
    if (actual !== expected)
        throw new Error("bad value" + msg + ": " + actual + ". Expected " + expected);
}

function shouldBeAsync(expected, run, msg) {
    let actual;
    var hadError = false;
    run().then(function(value) { actual = value; },
               function(error) { hadError = true; actual = error; });
    drainMicrotasks();

    if (hadError)
        throw actual;

    shouldBe(expected, actual, msg);
}

class BaseClass {
    baseClassValue() {
        return "BaseClassValue";
    }
    get property() {
        return "test!";
    }
}

class ChildClass extends BaseClass {
    asyncSuperProp() {
        return async x => super.baseClassValue();
    }
    asyncSuperProp2() {
        return async x => { return super.baseClassValue(); }
    }
}

shouldBeAsync("BaseClassValue", new ChildClass().asyncSuperProp());
shouldBeAsync("BaseClassValue", new ChildClass().asyncSuperProp2());

class ChildClass2 extends BaseClass {
    constructor() {
        return async (self = super()) => self.baseClassValue() + ' ' + super.property;
    }
}

shouldBeAsync("BaseClassValue test!", new ChildClass2());
