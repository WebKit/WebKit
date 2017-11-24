var abort = $vm.abort;

function assert(b) {
    if (!b) {
        abort(); 
    }
}
noInline(assert);

let test;

function f(eval) {
    assert(eval === test);
    eval(0x0);
    f(test);
}

for (let i = 0; i < 20; ++i) {
    test = function test() { return i; }
}

let error;
try {
    f(test);
} catch(e) {
    error = e;
}
assert(!!error);
assert(error instanceof RangeError);
