function makeInner() {
    return function inner(q) { return {}; };
}

function clobber(a,b,c,d,e,f,g,h) { return a; }

let escaped;
function sink(o) { escaped = o; }
noInline(sink);

let arr = [1.1,2.2,3.3,4.4,5.5,6.6,7.7,8.8];

function opt(ff, cond) {
    let v = {};
    let c = ff(0);
    c.x = v;
    clobber(...arr);
    if (cond)
        sink(c);
}
noInline(opt);

let inners = [makeInner(), makeInner()];
for (let i = 0; i < testLoopCount; ++i)
    opt(inners[i & 1], (i & 1) == 0);
opt(inners[0], true);
