"use strict";

function assert(a) {
    if (!a)
        throw new Error("Bad!");
}

var Base = class Base {
    constructor() { this._name = "Name"; }
    get name() { return this._name; } // If this instead returns a static: return "Foo" things somewhat work.
    set name(x) { this._name = x; }
};

var Subclass = class Subclass extends Base {
    get name() { return super.name; }
};

function getterName(instance) {
    return instance.name;
}

noInline(getterName);

function getterValue(instance) {
    return instance.value;
}

noInline(getterValue);

// Base case
var instance = new Subclass;
for (let i = 0; i < 10000;i++)
  assert(getterName(instance) == "Name");

// Polymorphic case

class PolymorphicSubclass {
    get value() { return super.value; }
};

let numPolymorphicClasses = 4;
let subclasses = new Array(numPolymorphicClasses);
for (let i = 0; i < numPolymorphicClasses; i++) {
    let BaseCode = `
       class Base${i} {
            get value() { return this._value; }
        };
    `;

    let Base = eval(BaseCode);
    subclasses[i] = new PolymorphicSubclass();
    subclasses[i]._value = i;

    Object.setPrototypeOf(subclasses[i], Base.prototype);
}

for (let i = 0; i < 1000000; i++) {
    let index = i % numPolymorphicClasses;
    let value = getterValue(subclasses[index]);
    assert(value == index);
}

// Megamorphic case

let nClasses = 1000;
class MegamorphicSubclass {
    get value() { return super.value; }
};

subclasses = new Array(nClasses);
for (let i = 0; i < nClasses; i++) {
    let BaseCode = `
       class Base${i + 4} {
            get value() { return this._value; }
        };
    `;

    let Base = eval(BaseCode);
    subclasses[i] = new MegamorphicSubclass();
    subclasses[i]._value = i;

    Object.setPrototypeOf(subclasses[i], Base.prototype);
}

for (let i = 0; i < 1000000; i++) {
    let index = i % nClasses;
    let value = getterValue(subclasses[index]);
    assert(value == index);
}

// CustomGetter case

let customGetter = createCustomGetterObject();
Object.setPrototypeOf(customGetter, Object.prototype);

let subObj = {
    __proto__: customGetter,
    get value () {
        return super.customGetterAccessor;
    }
}

for (let i = 0; i < 1000000; i++) {
    let value = getterValue(subObj);
    assert(value == 100);
}

subObj.shouldThrow = true;
for (let i = 0; i < 1000000; i++) {
    try {
        getterValue(subObj);
        assert(false);
    } catch(e) {
        assert(e instanceof TypeError);
    };
}

// CustomValue case

customGetter = createCustomGetterObject();
Object.setPrototypeOf(customGetter, Object.prototype);

subObj = {
    __proto__: customGetter,
    get value () {
        return super.customGetter;
    }
}

for (let i = 0; i < 1000000; i++) {
    let value = getterValue(subObj);
    assert(value == 100);
}

subObj.shouldThrow = true;
for (let i = 0; i < 1000000; i++) {
    let value = getterValue(subObj);
    assert(value == 100);
}

// Exception handling case

class BaseException {
    constructor() { this._name = "Name"; }
    get name() {
        if (this.shouldThrow)
            throw new Error("Forced Exception");
        return this._name;
    }
};

class SubclassException extends BaseException {
    get name() { return super.name; }
};

let eObj = new SubclassException;
for (let i = 0; i < 10000;i++)
  assert(getterName(eObj) == "Name");

eObj.shouldThrow = true;
for (let i = 0; i < 1000000; i++) {
    try {
        getterValue(eObj);
        assert(false);
    } catch(e) {
        eObj.shouldThrow = false;
        assert(getterName(eObj) == "Name");
    };
}

// In getter exception handling

class BaseExceptionComplex {
    constructor() { this._name = "Name"; }
    foo () {
        if (this.shouldThrow)
            throw new Error("Forced Exception");
    }
    get name() {
        this.foo();
        return this._name;
    }
};

class SubclassExceptionComplex extends BaseExceptionComplex {
    get name() {
        try {
            return super.name;
        } catch(e) {
            this.shouldThrow = false;
            return super.name;
        }
    }
};

eObj = new SubclassExceptionComplex;
for (let i = 0; i < 10000;i++)
  assert(getterName(eObj) == "Name");

eObj.shouldThrow = true;
for (let i = 0; i < 1000000; i++)
    assert(getterName(eObj) == "Name");

