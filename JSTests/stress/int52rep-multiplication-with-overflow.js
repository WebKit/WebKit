function foo() {
    const value = 2147483648;
    return (2147483648 * value);
}
noInline(foo);

let expected = 2147483648 * 2147483648;
for (let i = 0; i < 10000; i++) {
    let result = foo();
    if (result != expected)
        throw "FAIL";
}
