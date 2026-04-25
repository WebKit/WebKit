//@ skip if $buildType == "debug"
//@ slow!
//@ runDefault("--collectContinuously=1", "--slowPathAllocsBetweenGCs=100")

function foo(a) {
    a[Symbol()] = 1;
}
noInline(foo);

for (let i = 0; i < 1e7; i++) {
    let a = {};
    foo(a);
}
