function shouldBe(expected, actual, msg) {
    if (msg === void 0)
        msg = "";
    else
        msg = " for " + msg;
    if (actual !== expected)
        throw new Error("bad value" + msg + ": " + actual + ". Expected " + expected);
}

function shouldBeAsync(expected, run) {
    let actual;
    var hadError = false;
    run().then(function(value) { actual = value; },
               function(error) { hadError = true; actual = error; });
    drainMicrotasks();

    if (hadError)
        throw actual;

    shouldBe(expected, actual);
}

class BaseWrongClass {
    baseClassValue() {
        return "wrong #1";
    }
    get property() {
        return "wrong #2";
    }
}

function shouldBeAsyncAndStoreBind(expected, run) {
    shouldBeAsync(expected, run);
    shouldBeAsync(expected, run.bind({}));
    shouldBeAsync(expected, run.bind(1));
    shouldBeAsync(expected, run.bind(undefined));
    const obj = { 
        property : 'wrong value #1', 
        baseClassValue : () => 'worng value #2'
    };
    shouldBeAsync(expected, run.bind(obj));
    shouldBeAsync(expected, run.bind(new BaseWrongClass()));
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

var promiseHolder = {};
var promise = new Promise((resolve, reject) => {
    promiseHolder.resolve = resolve;
    promiseHolder.reject = reject;
});

class ChildClass4 extends BaseClass {
    constructor() {
        var arr = async () => {
            var doSomeStaff = () => {};
            doSomeStaff();
            await promise;
            return this.classValue() + ' ' + this.classProperty;
        };
        arr();
        super();
        this.internalValue = 'internalValue';
        return async () => {
            await 'await';
            promiseHolder.resolve();
            return this.classValue() + ' ' + this.classProperty;;
        };
    }
    classValue() {
        return "classValue";
    }
    get classProperty() {
        return "classProperty";
    }
}

shouldBeAsyncAndStoreBind("classValue classProperty", new ChildClass4());

class ChildClass5 extends BaseClass {
    constructor(result) {
        const arr = async () => this.id;
        arr().then(()=>{}, e => { result.error = e; });
    }
}

class ChildClass6 extends BaseClass {
    constructor(result) {
        const arr = async () => { 
            let z = this.id;
        };
        arr().then(()=>{}, e => { result.error = e; });
        super();
    }
}

class ChildClass7 extends BaseClass {
    constructor(result) {
        const arr = async () => this.id;
        arr().then(()=>{}, e => { result.error = e; });
        super();
    }
}

class ChildClass8 extends BaseClass {
    constructor(result) {
        const arr = async () => { let i  = this.id; super(); };
        arr().then(()=>{}, e => { result.error = e; });
    }
}

function checkTDZDuringCreate(klass) {
    let asyncError = {};
    try {
        var c = new klass(asyncError);
    } catch(e) {
        // We do not care about this error
    }
    drainMicrotasks();
    const error = asyncError.error instanceof ReferenceError && asyncError.error.toString() === 'ReferenceError: Cannot access uninitialized variable.';
    if (!error) throw new Error('TDZ error is expected, but appeared:' + asyncError.error);
}

checkTDZDuringCreate(ChildClass5);
checkTDZDuringCreate(ChildClass6);
checkTDZDuringCreate(ChildClass7);
checkTDZDuringCreate(ChildClass8);
