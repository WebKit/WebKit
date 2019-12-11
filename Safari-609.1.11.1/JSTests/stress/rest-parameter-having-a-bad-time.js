function assert(b) {
    if (!b)
        throw new Error("Bad!")
}
noInline(assert);

function test1() {
    function foo(...c) {
        return c;
    }
    noInline(foo);

    let arr = [1,2,3];
    for (let i = 0; i < 10000; i++) {
        let result = foo(...arr);
        assert(result.length === 3);
        assert(result.length === arr.length);
        assert(result[0] === arr[0]);
        assert(result[1] === arr[1]);
        assert(result[2] === arr[2]);
    }

    let called = false;
    Reflect.defineProperty(Array.prototype, "10", {
        get() { return 35; },
        set(x) { called = true; }
    });
    let called2 = false;
    Reflect.defineProperty(Array.prototype, "0", {
        get: function() { print("In get!"); return 35; },
        set: function(x) { called2 = true; }
    });

    for (let i = 0; i < 10000; i++) {
        let result = foo(...arr);
        assert(result.length === 3);
        assert(result[0] === arr[0]);
        assert(result[0] === 1);
        assert(result[1] === arr[1]);
        assert(result[2] === arr[2]);
        result[10] = 25;
        assert(result[10] === 35);
        assert(called);
        called = false;

        result[0] = "foo";
        assert(!called2); // Creating a rest should defineProperty, ensuring we don't call the setter.
    }

    for (let i = 0; i < 10000; i++) {
        let result = foo(...arr);
        assert(result.length === 3);
        assert(result[0] === arr[0]);
        assert(result[0] === 1);
        assert(result[1] === arr[1]);
        assert(result[2] === arr[2]);
        result[11] = 35;
        assert(result.length === 12);
        result[10] = 25;
        assert(result[10] === 35);
        assert(called);
        called = false;

        result[0] = "foo";
        assert(!called2); // Creating a rest should defineProperty, ensuring we don't call the setter.
    }
}
test1();
