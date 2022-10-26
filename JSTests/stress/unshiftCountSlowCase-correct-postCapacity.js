//@ if $buildType == "release" && !$memoryLimited then runDefault else skip end

function temp(i) {
    let a1 = [{}];
    a1.foo = 20;
    a1.foo1 = 20;
    a1.foo2 = 20;
    a1.foo3 = 20;
    a1.foo4 = 20;
    a1.foo5 = 20;
    a1.foo6 = 20;
    a1.foo8 = 20;
    a1.foo10 = 20;
    a1.foo11 = 20;
    delete a1[0];
    $vm.ensureArrayStorage(a1);
    try {
        let args = [-15, 1, 'foo', 20, 'bar'];
        for (let j = 0; j < i; ++j)
            args.push(j);
        for (let i = 0; i < 2**31 - 1; ++i) {
            Array.prototype.splice.apply(a1, args);
        }
    } catch(e) { }
}
let i = 62;
temp(i);
