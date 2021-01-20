const sym = Symbol();
function test() {
    Object.getPrototypeOf(false);
    Object.getPrototypeOf(-1);
    Object.getPrototypeOf("");
    Object.getPrototypeOf(sym);
    Object.getPrototypeOf(3n);
}
noInline(test);

for (let i = 0; i < 1e7; ++i)
    test();
