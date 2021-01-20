var ia = new Int8Array(1024);

function foo(o) {
    return o.f;
}

function bar(o) {

    try {
        o.f = 0x1;
        Uint8Array.prototype.find.call(ia, function () {
            o.f = 0x1;
        }, this);
    } catch (e) {
    }

    foo(o);
}

var o = new Object();
o.__defineGetter__("f", function () { });

for (var i = 0; i < 1000; ++i) {
    bar({});
    bar(o);
}
