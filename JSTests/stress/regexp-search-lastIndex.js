function test1() {
    const re = /x/g;
    re.lastIndex = 3;
    return "axbx".search(re);
}

function test2() {
    const re = /x/g;
    re.lastIndex = 42;
    "axbx".search(re);
    return re.lastIndex;
}

function test3() {
    const re = /x/y;
    re.lastIndex = 3;
    return "axbx".search(re);
}

const u1 = test1(), u2 = test2(), u3 = test3();

for (let i = 0; i < 300000; i++) {
    test1();
    test2();
    test3();
}

const o1 = test1(), o2 = test2(), o3 = test3();

if (o1 !== u1)
    throw o1 + " !== " + u1;

if (o2 !== u2)
    throw o2 + " !== " + u2;

if (o3 !== u3)
    throw o3 + " !== " + u3;
