var foo = [
    function(o) {
        var x = true;
        o.f.f;
        if (x)
            return;
        throw new Error();
    },
    function(o) {
        var x = true;
        o.f.f;
        if (!x)
            throw new Error();
        return;
    },
    function(o) {
        var x = 0;
        var y = 1;
        o.f.f;
        if (x < y)
            return;
        throw new Error();
    },
    function(o) {
        var x = 1;
        var y = 0;
        o.f.f;
        if (x > y)
            return;
        throw new Error();
    },
    function(o) {
        var x = 0;
        var y = 1;
        o.f.f;
        if (x <= y)
            return;
        throw new Error();
    },
    function(o) {
        var x = 1;
        var y = 0;
        o.f.f;
        if (x >= y)
            return;
        throw new Error();
    },
    function(o) {
        var x = 0;
        var y = 1;
        o.f.f;
        if (x >= y)
            throw new Error();
        return;
    },
    function(o) {
        var x = 1;
        var y = 0;
        o.f.f;
        if (x <= y)
            throw new Error();
        return;
    },
    function(o) {
        var x = 0;
        var y = 1;
        o.f.f;
        if (x > y)
            throw new Error();
        return;
    },
    function(o) {
        var x = 1;
        var y = 0;
        o.f.f;
        if (x < y)
            throw new Error();
        return;
    },
    function(o) {
        var x = 42;
        o.f.f;
        if (x == 42)
            return;
        throw new Error();
    },
    function(o) {
        var x = 42;
        o.f.f;
        if (x != 42)
            throw new Error();
        return;
    },
    function(o) {
        var x = 42;
        o.f.f;
        if (x === 42)
            return;
        throw new Error();
    },
    function(o) {
        var x = 42;
        o.f.f;
        if (x !== 42)
            throw new Error();
        return;
    },
];
for (var i = 0; i < foo.length; ++i)
    noInline(foo[i]);

function test(o) {
    var failed = [];
    for (var i = 0; i < foo.length; ++i) {
        try {
            foo[i](o);
        } catch (e) {
            failed.push("Failed " + foo[i] + " with " + e);
        }
    }
    if (failed.length)
        throw failed;
}

var object = {f:{f:42}};

for (var i = 0; i < 10000; ++i) {
    test(object);
}

test({f:{g:43}});

