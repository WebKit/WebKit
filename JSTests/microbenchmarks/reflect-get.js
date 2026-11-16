(function() {
    var target = { foo: 1 };

    var foo;
    for (var i = 0; i < 1e7; i++)
        foo = Reflect.get(target, "foo");

    if (foo !== 1)
        throw new Error("Bad assertion!");
})();
