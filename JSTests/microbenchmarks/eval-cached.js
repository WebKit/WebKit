function foo(string) {
    let result = 42;
    for (let i = 0; i < 1000; ++i)
        eval(string);
    return result;
}

noInline(foo);

for (let i = 0; i < 200; ++i) {
    let result = foo("++result");
    if (result != 42 + 1000)
        throw "Error: bad result: " + result;
}
