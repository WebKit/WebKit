//@ skip

function makeBoundFunc(f) {
    return f.bind(f);
}
noInline(makeBoundFunc);

function foo() {
    function f() { }
    for (let i = 0; i < 400000; i++) {
        makeBoundFunc(f); 
    }
}
foo();
