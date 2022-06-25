function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}

(function (x = 20) {
    var a;
    assert(arguments.length === 0);
    assert(typeof arguments !== "function");
    {
        function arguments() {
        }

        function b() {
            var g = 1;
            a[5];
        }
    }
    assert(typeof arguments === "function");
}());

(function (x = () => arguments) {
    var a;
    let originalArguments = x();
    assert(originalArguments === arguments);
    let z;
    {
        function arguments() { return 25; }
        z = arguments;

        function b() {
            var g = 1;
            a[5];
        }
    }
    assert(z !== originalArguments);
    assert(x() === z);
    assert(typeof z === "function");
    assert(z() === 25);
}());

(function ({arguments}) {
    assert(arguments === 20);

    var a;
    {
        function arguments() { return 25; }
        assert(arguments() === 25);

        function b() {
            var g = 1;
            a[5];
        }
    }

    assert(arguments === 20);
}({arguments: 20}));

(function (y = () => arguments, {arguments}) {
    assert(y() === arguments);
    var a;
    {
        function arguments() { return 25; }
        assert(arguments() === 25);
        assert(y() !== arguments);

        function b() {
            var g = 1;
            a[5];
        }
    }

    assert(y() === arguments);
}(undefined, {arguments: {}}));

(function (y = () => arguments, z = y(), {arguments}) {
    assert(typeof z === "object");
    assert(z.length === 3);
    assert(z[0] === undefined);
    assert(z[1] === undefined);
    assert(typeof z[2] === "object");
    assert(z[2].arguments === arguments);
    assert(y() === arguments);

    var a;
    {
        function arguments() { return 25; }
        assert(arguments() === 25);
        assert(y() !== arguments);

        function b() {
            var g = 1;
            a[5];
        }
    }

    assert(y() === arguments);
}(undefined, undefined, {arguments: {}}));

(function (arguments) {
    assert(arguments === 25);

    var a;
    {
        function arguments() { return 30; }
        assert(arguments() === 30);

        function b() {
            var g = 1;
            a[5];
        }
    }

    assert(arguments === 25);
}(25));

(function (arguments) {
    assert(arguments === 25);
    let foo = () => arguments;
    assert(foo() === arguments);

    var a;
    {
        function arguments() { return 30; }
        assert(arguments() === 30);
        assert(foo() === 25);

        function b() {
            var g = 1;
            a[5];
        }
    }

    assert(arguments === 25);
    assert(foo() === arguments);
}(25));

(function () {
    assert(arguments.length === 1);
    assert(arguments[0] === 25);

    let outer = () => arguments;
    var a;
    {
        assert(outer()[0] === 25);
        function arguments() { return 30; }
        assert(outer() === arguments);
        assert(outer()() === 30);
        assert(arguments() === 30);

        function b() {
            var g = 1;
            a[5];
        }
    }

    assert(arguments() === 30);
}(25));
