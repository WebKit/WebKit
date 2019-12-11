//@ runDefault("--jitPolicyScale=0")

function foo() {
    let j = 0;
    let arr = [0];
    arr.foo = 0;
    for (var i = 0; i < 1024; i++) {
        arr[0] = new Array(1024);
    }
}

foo();
foo();
