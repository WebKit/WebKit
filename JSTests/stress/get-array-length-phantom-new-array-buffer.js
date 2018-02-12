function effects() {}
noInline(effects);

function foo() {
    let x = [1,2,3];
    effects();
    return x.length;
}
noInline(foo);

for (let i = 0; i < 100000; ++i) {
    if (foo() !== 3)
        throw new Error();
}
