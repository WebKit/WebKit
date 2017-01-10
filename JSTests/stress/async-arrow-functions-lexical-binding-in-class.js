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

function runSomething(callback) {
    callback();
}

class ChildClass extends BaseClass {
    classValue() {
        return "classValue";
    }
    get classProperty() {
        return "classProperty";
    }
    asyncValueExp() {
        return async x => 'value';
    }
    asyncValueBody() {
        return async x => { return 'value'; };
    }
    asyncThisPropExp() {
        return async x => this.classProperty;
    }
    asyncThisPropBody() {
        return async x => { return this.classProperty; };
    }
    asyncThisPropInEvalExp() {
        return async x => eval('this.classProperty');
    }
    asyncThisPropInEvalBody() {
        return async x => { return eval('this.classProperty'); };
    }
    asyncThisValueExp() {
        return async x => this.classValue();
    }
    asyncThisValueBody() {
        return async x => { return this.classValue(); };
    }
    asyncThisValueInEvalExp() {
        return async x => eval('this.classValue()');
    }
    asyncThisValueInEvalBody() {
        return async x => { return eval('this.classValue()'); };
    }
}

shouldBeAsync("value", new ChildClass().asyncValueExp());
shouldBeAsync("value", new ChildClass().asyncValueBody());

shouldBeAsync("classProperty", new ChildClass().asyncThisPropExp());
shouldBeAsync("classProperty", new ChildClass().asyncThisPropBody());

shouldBeAsync("classProperty", new ChildClass().asyncThisPropInEvalExp());
shouldBeAsync("classProperty", new ChildClass().asyncThisPropInEvalBody());

shouldBeAsync("classValue", new ChildClass().asyncThisValueExp());
shouldBeAsync("classValue", new ChildClass().asyncThisValueBody());

shouldBeAsync("classValue", new ChildClass().asyncThisValueInEvalExp());
shouldBeAsync("classValue", new ChildClass().asyncThisValueInEvalBody());

class ChildClass2 extends BaseClass {
    constructor() {
        super();
        return async () => this.classValue() + ' ' + this.classProperty;
    }
    classValue() {
        return "classValue";
    }
    get classProperty() {
        return "classProperty";
    }
}

shouldBeAsync("classValue classProperty", new ChildClass2());
