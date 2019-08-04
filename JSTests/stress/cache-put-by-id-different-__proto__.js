//@ runNoLLInt

let counter = 0;
class A {
    static set y(x) {
        if (counter++ === 9) {
            C.__proto__ = B2;
        }
    }
}

class B1 extends A {
}

let calledB2 = false;
class B2 extends A {
    static set y(x) {
        calledB2 = true;
    }
}

class C extends B1 {
}

class D extends C {
}

for (let i = 0; i < 11; i++) {
    D.y = 42;
}

if (!calledB2)
    throw new Error('The setter for B2.y should have been called');
