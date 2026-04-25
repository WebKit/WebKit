//@ runDefault("--jitPolicyScale=0")
let var1 = 0x80000000;

function foo() {
    for (let i = 0; i < 7; i++) {
        var1 = var1 << i;
        var1 = var1 - 1;
    }
    var1 >>= 8;
    return var1;
}
noInline(foo);

function main() {
    let ret = undefined;
    foo();
    let expected = foo();
    for (let i = 0; i < 1e3; i++) {
        ret = foo();
        if (expected != ret)
            throw new Error();
    }
}
main();
