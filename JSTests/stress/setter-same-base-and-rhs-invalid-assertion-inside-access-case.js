function ctor() {}
ctor.prototype.__defineSetter__("f", function () { });

function theFunc(o) {
    o.f = o;
}
noInline(theFunc);
function run(o) {
    theFunc(o);
}

for (let i = 0; i < 100000; ++i) {
    run(new ctor())
    let o = new ctor();
    o.g = 54;
    run(o);
}
