let a = {};
a.foo = 1;
a.bar = 1.1;
a.other = 4.4;

let b = {};
b.baz = 3.3;
b.foo = 1;
b.other = 5.4;

let c = {};
c.other = 3.4;
c.other2 = 9.1;
c.foo = 1;

function func(flag1, flag2) {
    let tmp = a;
    if (flag1) {
        tmp = b;
        if (flag2)
            tmp = c;
    }


    let x;
    for (let i = 0; i < 10; ++i) {
        if (flag1)
            x = tmp.foo;
        else
            x = tmp.foo;
    }
    return x;
}

for (let i = 0; i < 1e5; ++i) {
    if (func(i % 2, i % 3) !== 1)
        throw new Error();
}
