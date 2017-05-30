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

var error = undefined;
var value = undefined;

class A {
    constructor() {
        this._id = 'class-id';
    }
}

const childA1 = new class extends A {
  constructor() {
    var f = async (a=super()) => { return 'abc'; }
    f().then( val => {value = val; }, err => { error = err;});
  }
}

drainMicrotasks();

shouldBe(childA1._id, 'class-id');
shouldBe(value, 'abc');
shouldBe(error, undefined);

value = undefined;
error = undefined;

const childA2 = new class extends A {
  constructor() {
    var f = async (a) => { super(); return 'abc'; }
    f().then( val => {value = val; }, err => { error = err;});
  }
}

drainMicrotasks();

shouldBe(childA2._id, 'class-id');
shouldBe(value, 'abc');
shouldBe(error, undefined);

value = undefined;
error = undefined;

const childA3 = new class extends A {
    constructor() {
        var f = async (a = super()) => { super(); return 'abc'; }
        f().then( val => {value = val; }, err => { error = err;});
    }
}

drainMicrotasks();

shouldBe(childA3._id, 'class-id');
shouldBe(value, undefined);
shouldBe(error.toString(), 'ReferenceError: \'super()\' can\'t be called more than once in a constructor.');


let childA4;
let catchError;
error = undefined; 
try {
    childA4 = new class extends A {
        constructor() {
            var f = async (a) => { await 'await value'; super(); return 'abc'; }
            f().then(val => { value = val; }, err => { error = err; });
        }
    }
} catch (err) {
    catchError = err;  
}

drainMicrotasks();

shouldBe(childA4, undefined);
shouldBe(value, 'abc');
shouldBe(error, undefined);
shouldBe(catchError.toString(), 'ReferenceError: Cannot access uninitialized variable.');

catchError = undefined;
error = undefined; 
value = undefined;

const childA5 = new class extends A {
    constructor() {
        var f = async (a) => { super(); await 'await value'; return 'abc'; }
        f().then(val => { value = val; }, err => { error = err; });
    }
}

drainMicrotasks();

shouldBe(childA5._id, 'class-id');
shouldBe(value, 'abc');
shouldBe(error, undefined);
shouldBe(catchError, undefined);

function checkClass(classSource) {
    let base1 = undefined;
    let error = undefined; 
    let value = undefined;
    let catchError = undefined;
    try {
        base1 = eval(classSource);

        drainMicrotasks();
    } catch (err) {
        catchError = err;  
    }

    shouldBe(base1, undefined);
    shouldBe(value, undefined);
    shouldBe(error, undefined);
    shouldBe(catchError.toString(), 'SyntaxError: super is not valid in this context.');
}

checkClass(`new class {
    constructor() {
        var f = async (a) => { super(); return 'abc'; }
        f().then(val => { value = val; }, err => { error = err; });
    }
}`);

checkClass(`new class {
    constructor() {
        var f = async (a) => { await 'p'; super(); return 'abc'; }
        f().then(val => { value = val; }, err => { error = err; });
    }
}`);

checkClass(`new class {
    constructor() {
        var f = async (a) => { super(); await 'p'; return 'abc'; }
        f().then(val => { value = val; }, err => { error = err; });
    }
}`);


checkClass(`new class extends A {
    method() {
        var f = async (a) => { super(); return 'abc'; }
        f().then(val => { value = val; }, err => { error = err; });
    }
}`);

checkClass(`new class extends A {
    get prop() {
        var f = async (a) => { super(); return 'abc'; }
        f().then(val => { value = val; }, err => { error = err; });
    }
}`);

checkClass(`new class extends A {
    set prop(_value) {
        var f = async (a) => { super(); return 'abc'; }
        f().then(val => { value = val; }, err => { error = err; });
    }
}`);