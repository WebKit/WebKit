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

class BaseWrongClassClass {
    baseClassValue() {
        return "wrong #1";
    }
    get property() {
        return "wrong #2";
    }
}

function shouldBeAsyncAndStoreBind(expected, run, msg) {
    shouldBeAsync(expected, run, msg);
    shouldBeAsync(expected, run.bind({}), msg);
    shouldBeAsync(expected, run.bind(1), msg);
    shouldBeAsync(expected, run.bind(undefined), msg);
    const obj = { 
        property : 'wrong value #1', 
        baseClassValue : () => 'worng value #2'
    };
    shouldBeAsync(expected, run.bind(obj), msg);
    shouldBeAsync(expected, run.bind(new BaseWrongClassClass()), msg);
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
    asyncThisPropWithAwaitBody() {
        return async x => { 
            var self = this.classProperty; 
            self = await 'abc';  
            return this.classProperty; 
        };
    }
    asyncThisPropInEvalExp() {
        return async x => eval('this.classProperty');
    }
    asyncThisPropInEvalBody() {
        return async x => { return eval('this.classProperty'); };
    }
    asyncThisPropInEvalWithAwaitBody() {
        return async x => { 
            var self = eval('this.classProperty');
            await 'abc';
            return eval('this.classProperty'); 
        };
    }
    asyncThisValueExp() {
        return async x => this.classValue();
    }
    asyncThisValueBody() {
        return async x => { return this.classValue(); };
    }
    asyncThisValueBodyWithAwait() {
        return async x => { 
            var self = this.classValue();
            await 'self'; 
            return this.classValue(); 
        };
    }
    asyncThisValueInEvalExp() {
        return async x => eval('this.classValue()');
    }
    asyncThisValueInEvalBody() {
        return async x => { return eval('this.classValue()'); };
    }
    asyncThisValueInEvalWithAwaitBody() {
        return async x => { 
            var self = eval('this.classValue()');
            await 'self'; 
            return eval('this.classValue()'); 
        };
    }
}

shouldBeAsyncAndStoreBind("value", new ChildClass().asyncValueExp());
shouldBeAsyncAndStoreBind("value", new ChildClass().asyncValueBody());

shouldBeAsyncAndStoreBind("classProperty", new ChildClass().asyncThisPropExp());
shouldBeAsyncAndStoreBind("classProperty", new ChildClass().asyncThisPropBody());
shouldBeAsyncAndStoreBind("classProperty", new ChildClass().asyncThisPropWithAwaitBody());
shouldBeAsyncAndStoreBind("classProperty", new ChildClass().asyncThisPropWithAwaitBody());

shouldBeAsyncAndStoreBind("classProperty", new ChildClass().asyncThisPropInEvalExp());
shouldBeAsyncAndStoreBind("classProperty", new ChildClass().asyncThisPropInEvalBody());
shouldBeAsyncAndStoreBind("classProperty", new ChildClass().asyncThisPropInEvalWithAwaitBody());

shouldBeAsyncAndStoreBind("classValue", new ChildClass().asyncThisValueExp());
shouldBeAsyncAndStoreBind("classValue", new ChildClass().asyncThisValueBody());
shouldBeAsyncAndStoreBind("classValue", new ChildClass().asyncThisValueBodyWithAwait());

shouldBeAsyncAndStoreBind("classValue", new ChildClass().asyncThisValueInEvalExp());
shouldBeAsyncAndStoreBind("classValue", new ChildClass().asyncThisValueInEvalBody());
shouldBeAsyncAndStoreBind("classValue", new ChildClass().asyncThisValueInEvalWithAwaitBody());

class ChildClass2 extends BaseClass {
    constructor() {
        super();
        this.value = 'internalValue';
        return async () => this.classValue() + ' ' + this.classProperty;
    }
    classStaticValue() {
        return "classStaticValue";
    }
    classValue() {
        return this.value;
    }
    get classProperty() {
        return "classProperty";
    }
}

shouldBeAsyncAndStoreBind("internalValue classProperty", new ChildClass2());

class ChildClass3 extends BaseClass {
    constructor() {
        super();
        this.internalValue = 'internalValue';
        return async () => {
            var self = this.classValue() + ' ' + this.classProperty;
            await 'self';
            return this.classValue() + ' ' + this.classProperty;
        }
    }
    classValue() {
        return "classValue";
    }
    get classProperty() {
        return "classProperty";
    }
}

shouldBeAsyncAndStoreBind("classValue classProperty", new ChildClass3());
