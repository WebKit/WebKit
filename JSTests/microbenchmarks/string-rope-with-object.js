function body() {
    function f(s,t) {
        let total = 0;
        for (let i = 0; i < 2000; i++) {
            total += i;
        }
        return "a" + "b" + s + t;
    }
    noInline(f);

    var foo = new String(10);
    String.prototype.valueOf = function() { return 1; };

    var count = 0;
    var bar = { valueOf: function() { return count++; } };
    (function wrapper() {
        for (var i = 0; i < 10000; i++) {
            if (f(foo,bar) !== "ab1" + i)
                throw "bad";
        }
    })();

}
body();
