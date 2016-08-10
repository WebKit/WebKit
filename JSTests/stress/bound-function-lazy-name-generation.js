function assert(b) {
    if (!b)
        throw new Error("Bad assertion!");
}

function test() {
    let f = function foo() { }.bind({});
    assert(f.name === "bound foo");

    f = function () { }.bind({});
    assert(f.name === "bound ");

    f = function foo() { }.bind({});
    assert(Reflect.ownKeys(f).includes("name"));
    assert(f.name === "bound foo");
    assert(Reflect.ownKeys(f).includes("name"));

    f = function foo() { }.bind({});
    assert(f.name === "bound foo");
    assert(Reflect.ownKeys(f).includes("name"));
}
for (let i = 0; i < 10000; i++)
    test();
