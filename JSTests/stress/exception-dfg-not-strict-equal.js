;(function () {
function foo(a, b) {
    var result = null;
    try {
        result = a == b;
    } catch(e) {
    }
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

var o = {
    valueOf: function() { return {}; },
    toString: function() { return {}; }
};
foo(o, "hello");
})();


function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}
noInline(assert);


;(function() {

var _shouldDoSomethingInFinally = false;
function shouldDoSomethingInFinally() { return _shouldDoSomethingInFinally; }
noInline(shouldDoSomethingInFinally);

function foo(a, b) {
    var result = null;
    try {
        result = a == b;
    } finally {
        if (shouldDoSomethingInFinally())
            assert(result === null);
    }
    return result;
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

var o = {
    valueOf: function() { return {}; },
    toString: function() { return {}; }
};
try {
    _shouldDoSomethingInFinally = true;
    foo(o, "hello");
} catch(e) {}

})();
