(function() {
    var target = new (class {});

    for (var i = 0; i < 1e6; i++)
        Reflect.set(target, "foo", 1, {});
})();
