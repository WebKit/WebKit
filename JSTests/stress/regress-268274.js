function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

(function() {
    var obj = {};

    function foo(a, b, [foo = function() { return e; }], ...[e, {d = foo()}]) { 
        var d;
        assert(d === 20);
        return d;
    }

    noInline(foo);

    for (var i = 0; i < 5000; i++) {
        assert(foo(null, obj, [], 20, {}) === 20);
    }
})();

(function() {
    function foo(f1 = function(x) { b = x; }, ...[f2 = function() { return b; }, {b}]) {
        var b;
        assert(b === 34);
        assert(f2() === 34);
        f1(50);
        assert(b === 34);
        assert(f2() === 50);
    }

    noInline(foo);

    for (var i = 0; i < 5000; i++) {
        foo(undefined, undefined, {b: 34});
    }
})();

(function() {
    var outer = "outer";

    function foo(...[a = outer, b = function() { return a; }, c = function(v) { a = v; }]) {
        var a;
        assert(a === "outer");
        a = 20;
        assert(a === 20);
        assert(b() === "outer");
        c("hello");
        assert(b() === "hello");
    }

    function bar(a = outer, b = function() { return a; }, ...[c = function(v) { a = v; }]) {
        with ({}) {
            var a;
            assert(a === "outer");
            a = 20;
            assert(a === 20);
            assert(b() === "outer");
            c("hello");
            assert(b() === "hello");
        }
    }

    noInline(foo);
    noInline(bar);

    for (var i = 0; i < 2500; i++) {
        foo();
        bar();
    }
})();
