function constStr() {
    return ' ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ';
}

function foo(z) {
    switch (z) {
    case 'a':
    case 'a':
    case 'a':
        return 1;
    default:
        return 2;
    }
}
noInline(foo);

for (let i = 0; i < 10000; ++i) {
    let str = 'a' + constStr();
    let result = foo(str);
    if (result !== 2)
        throw new Error("Bad result");
}
