//@ $skipModes << :lockdown if $buildType == "debug"

function f(x, y, z) {
    // comment in the body
    const w = 42;
    return w + x + y + z;

}
noInline(f);

class C {
    // comment in the class
    constructor() {
    }

    method1() {
        return "some string";
    }

    method2() {
        return 42;
    }
}

function test() {
    f.toString();
    C.toString();
    print.toString();
}
noInline(test);

for (let i = 0; i < 1e7; ++i)
    test();

function test2(x, y, z) {
    for (let i = 0; i < 1e6; ++i) {
        f(x.toString(), y.toString(), z.toString(), x.toString(), y.toString());
    }
}
noInline(test2);

test2(f, C, print);
