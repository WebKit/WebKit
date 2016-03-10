var passed = true;
try {
    eval("new.target;");
    passed = false;
} catch(e) {}

test(passed, true, "new.target cannot be called in global scope");

// Test without class syntax

function test(result, expected, message) {
    if (result !== expected)
        throw "Error: " + message + ". was: " + result + " wanted: " + expected;
}

function call() {
    test(new.target, undefined, "new.target should be undefined in a function call");
}
call();

function Constructor() {
    test(new.target, Constructor, "new.target should be the same as constructor");
    function subCall() {
        test(new.target, undefined, "new.target should be undefined in a sub function call");
    }
    subCall();

    function SubConstructor() {
        test(new.target, SubConstructor, "new.target should be subConstructor");
    }
    new SubConstructor();

}
new Constructor();

// This is mostly to test that calling new on new.target deos the right thing.
function doWeirdThings(arg) {
    if (new.target) {
        if (arg)
            this.value = new.target(1);
        else
            this.value = new new.target(true);
    } else
        return arg;
}

test(new doWeirdThings(false).value.value, 1, "calling new on new.target did something weird");

// Test with class syntax

class SuperClass {
    constructor() {
        this.target = new.target;
    }
}

class SubClass extends SuperClass {
    constructor() {
        super();
    }
}

test(new SuperClass().target, SuperClass, "new.target should be the same as the class constructor");
test(new SubClass().target, SubClass, "new.target should not change when passed through super()");
