function foo() {
    "foo".replace(/f/g, "");
    return "foo".replace(/f/g, 42);
}
noInline(foo);

let result;
for (let i = 0; i < 10000; i++) {
    result = foo();
    if (result !== "42oo")
        throw new Error("Error: bad result: " + result);
}

