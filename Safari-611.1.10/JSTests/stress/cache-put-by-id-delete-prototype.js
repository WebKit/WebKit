//@ runNoLLInt

let calledA = false;
let counter = 0;

class A {
    set y(_) {
        calledA = true;
    }
}

class B extends A {
    set y(_) {
        if (counter++ === 9)
            delete B.prototype.y;
    }
}
class C extends B { }

let c = new C();
for (let i = 0; i < 11; i++) {
    c.y = 42;
}

if (!calledA)
    throw new Error('The setter for A.y should have been called');
