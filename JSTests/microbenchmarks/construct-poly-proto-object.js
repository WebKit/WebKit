//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo() {
    class A {
        constructor() {
            this.x = 25;
            this.y = 30;
        }
    };
    return A;
}
let A = foo();
let B = foo();
noInline(A);
noInline(B);

for (let i = 0; i < 400000; ++i) {
    let b = !!(i % 2);
    if (b)
        new A;
    else
        new B;
}
