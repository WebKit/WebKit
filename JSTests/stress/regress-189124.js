//@ runDefault("--jitPolicyScale=0")

function makeTmp() {
    let tmp = {a: 1};
    gc();
    tmp.__proto__ = {};
    return tmp;
}

function foo(tmp, obj) {
    for (let k in tmp) {
        tmp.__proto__ = {};
        gc();
        obj.__proto__ = {};

        var result = obj[k];
        return result;
    }
}

foo(makeTmp(), {});

let memory = new Uint32Array(100);
memory[0] = 0x1234;

let fooResult = foo(makeTmp(), memory);
var result = $vm.value(fooResult);

if (result != "Undefined")
    throw "FAIL";

