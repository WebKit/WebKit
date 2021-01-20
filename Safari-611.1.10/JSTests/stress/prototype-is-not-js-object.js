function foo() {
    function bar() {
        this.x = 42;
    }
    bar.prototype = 50;
    return new bar();
}

function assert(b) {
    if (!b)
        throw new Error("Bad");
}

let items = [
    foo(),
    foo(),
    foo(),
    foo(),
    foo(),
    foo(),
];

function validate(item) {
    assert(item.notThere === undefined);
    assert(item.x === 42);
    assert(item.__proto__ === Object.prototype);
}

for (let i = 0; i < 10000; ++i) {
    for (let item of items)
        validate(item);
}
