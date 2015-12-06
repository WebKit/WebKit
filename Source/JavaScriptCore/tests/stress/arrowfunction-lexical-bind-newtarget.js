var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

function getTarget(name) {
    return x => new.target;
}

noInline(getTarget)

for (var i=0; i < 1000; i++) {
    var undefinedTarget = getTarget()();
    testCase(undefinedTarget, undefined, "Error: new.target is not lexically binded inside of the arrow function #1");
}

for (var i = 0; i < 1000; i++) {
    var newTarget = new getTarget()();
    testCase(newTarget, getTarget, "Error: new.target is not lexically binded inside of the arrow function #2");
}

var passed = false;
var A = class A {
    constructor() {
        this.idValue = 123;
        passed = passed && new.target === B;
    }
};

var B  = class B extends A {
    constructor() {
        var f = () => {
            passed = new.target === B;
            super();
        };
        f();
    }
};

for (var i = 0; i < 1000; i++) {
    passed = false;
    var b = new B();

    testCase(passed, true, "Error: new.target is not lexically binded inside of the arrow function in constructor #3");
}

var C = class C extends A {
    constructor(tryToAccessToVarInArrow) {
        var f = () => {
            super();
            if (tryToAccessToVarInArrow)
                this.id2 = newTargetLocal;
        };

        f();

        if (!tryToAccessToVarInArrow)
            this.id = newTargetLocal;
    }
};

var tryToCreateClass = function (val) {
    var result = false;
    try {
        new C(val);
    }
    catch (e) {
        result = e instanceof ReferenceError; 
    }

    return result;
};

for (var i = 0; i < 1000; i++) {
    testCase(tryToCreateClass(true), true, "Error: newTargetLocal should be hided variable");
    testCase(tryToCreateClass(false), true, "Error: newTargetLocal should be hided variable");
}
