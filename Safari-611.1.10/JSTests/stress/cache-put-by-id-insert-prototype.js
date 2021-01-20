//@ runNoLLInt

let calledB = false;
let counter = 0;

class A {
    set y(_) {
        if (counter++ === 9) {
            Object.defineProperty(B.prototype, 'y', {
                set(_) {
                    calledB = true;
                }
            });
        }
    }
}

class B extends A { }
class C extends B { }

let c = new C();
for (let i = 0; i < 11; i++) {
    c.y = 42;
}

if (!calledB)
    throw new Error('The setter for B.y should have been called');
