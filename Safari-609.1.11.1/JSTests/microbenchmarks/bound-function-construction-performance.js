//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
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
