let foo = "get some property storage";
let first = "new first element";
let bar = "ensure property storage is zeroed";

function run(array) {
    array.foo = foo;
    array.unshift(first, ...new Array(100));
    array.bar = bar;
    return array;
}
noInline(run);

function test() {
    let array = run([]);
    if (array.foo !== foo)
        throw new Error();
    if (array.bar !== bar)
        throw new Error();
    if (array[0] !== first)
        throw new Error();

    array = [];
    array.unshift(1);
    array = run(array);
    if (array.foo !== foo)
        throw new Error();
    if (array.bar !== bar)
        throw new Error();
    if (array[0] !== first)
        throw new Error();
}

for (let i = 0; i < 1; i++)
    test();
