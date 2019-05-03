function X(){}

X.prototype.__defineSetter__('f', function* () {
    arguments;
});

function foo(o) {
    i+'';
    for (var i = 0; i < 1000; ++i) {
        o.f = 0;
    }
}
noInline(foo);

for (let i = 0; i < 1000; ++i) {
    foo(new X());
}
