(function() {
    var target = { get foo() { return this.bar; }, bar: 1 };
    var receiver = { bar: 2 };

    var bar;
    for (var i = 0; i < 1e7; i++)
        bar = Reflect.get(target, "foo", receiver);

    if (bar !== 2)
        throw new Error("Bad assertion!");
})();
