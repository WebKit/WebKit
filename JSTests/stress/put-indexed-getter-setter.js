function foo() {
    let setterValue = 0;
    class X {
        static set 7(x) { setterValue = x; }
        static get 7() { }
    };
    X[7] = 27;
    if (setterValue !== 27)
        throw new Error("Bad")
}
noInline(foo);

for (let i = 0; i < 10000; ++i)
    foo();

Object.defineProperty(Object.prototype, "7", {get() { return 500; }, set(x) { }}); // this shouldn't change the test at all, it should be doing defineOwnProperty.
for (let i = 0; i < 10000; ++i)
    foo();
