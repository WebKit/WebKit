function assert(a) {
    if (!a)
        throw new Error("Bad Assertion!");
}

class A {
    constructor(prop) {
        this.prop = prop;
    }
    call() {
        return this.prop;
    }
    apply() {
        return this.prop;
    }
}

class B extends A {
  testSuper() {
    assert(super.call() == 'value');
    assert(super.apply() == 'value');
  }
}

const obj = new B('value')
obj.testSuper()

class C {}

class D extends C {
    testSuper() {
        try {
            super.call();
            assert(false);
        } catch(e) {
            assert(e.message == "super.call is not a function. (In 'super.call()', 'super.call' is undefined)");
        }
        
        try {
            super.apply();
            assert(false);
        } catch(e) {
            assert(e.message == "super.apply is not a function. (In 'super.apply()', 'super.apply' is undefined)");
        }
    }
}

let d = new D();
d.testSuper();

