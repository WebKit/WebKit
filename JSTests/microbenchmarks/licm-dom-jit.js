function assert(b) { 
    if (!b)
        throw new Error;
}

function foo(o) {
    let r = 0;
    for (let i = 0; i < 100; ++i) {
        r = o.func();
    }
    return r;
}
noInline(foo);

function bar(o) {
    let r = 0;
    for (let i = 0; i < 100; ++i) {
        r = o.customGetter;
    }
    return r;
}
noInline(bar);


let o1 = $vm.createDOMJITFunctionObject();
let o2 = $vm.createDOMJITGetterNoEffectsObject();

for (let i = 0; i < 100000; ++i) {
    assert(foo(o1) === 42);
    assert(bar(o2) === 42);
}
