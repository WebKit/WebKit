function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}
noInline(assert);
var o = {
    valueOf: function() { return {}; },
    toString: function() { return {}; }
};
function read() {
    return "read";
}
noInline(read);

function foo(a, b) {
    var result = null;
    var value = read();
    try {
        result = a == b;
    } catch(e) {
        assert("" + value === "read");
    }
    return value;
}
noInline(foo);

for (var i = 0; i < 1000; i++) {
    foo(10, 20);
    foo({}, {});
    foo(10, 10.0);
    foo("hello", "hello");
    foo(null, undefined);
    foo(false, 0);
}
foo(o, "hello");
