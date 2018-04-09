function f() {
    var a = Array(100).fill(0);
    var ta = new Set(a.map((v,k)=>k));
    var xs = [a, ta];
    var q = 0;
    var t = Date.now();
    for (var i = 0; i < 100000; ++i) {
        for (var x of xs[i&1]) q+=x;
    }
    return [Date.now()-t,q];
}
noInline(f);
f();

