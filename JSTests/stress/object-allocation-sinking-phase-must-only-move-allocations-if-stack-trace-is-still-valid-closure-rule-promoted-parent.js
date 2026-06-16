function makeInner() {
    return function inner(q) { return {}; };
}

function clobber(a,b,c,d,e,f,g,h) { return a; }

let escaped;
function sink(o) { escaped = o; }
noInline(sink);

let arr = [1.1,2.2,3.3,4.4,5.5,6.6,7.7,8.8];

function opt(ff) {
    let p = ff(0);
    let y = {};
    p.f = y;
    p.f = null;
    clobber(...arr);
    sink(p);
}
noInline(opt);

let inners = [makeInner(), makeInner()];
for (let i = 0; i < testLoopCount; ++i)
    opt(inners[i & 1]);
opt(inners[0]);
