let a = {};
a.foo = 1;
a.bar = 1.1;

let b = {};
b.baz = 3.3;
b.foo = 1;

function func(flag) {
    let tmp = flag ? a : b;

    let x;
    for (let i = 0; i < 10; ++i) {
        if (flag)
            x = tmp.foo;
        else
            x = tmp.foo;
    }
    return x;
}

for (let i = 0; i < 1e5; ++i) {
    if (func(i % 2) !== 1)
        throw new Error();
}
