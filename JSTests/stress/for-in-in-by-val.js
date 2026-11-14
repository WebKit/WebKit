function assert(b, m) {
    if (!b)
        throw new Error(m);
}

function has(o, p) {
    return p in o;
}

function countIn(o) {
    let present = 0;
    for (const p in o) {
        if (Reflect.has(o, p))
            present++;
    }
    return present;
}
noInline(countIn);

function countInHelper(o) {
    let present = 0;
    for (const p in o) {
        if (has(o, p))
            present++;
    }
    return present;
}
noInline(countInHelper);

function crossCheck(o, other) {
    let n = 0;
    for (const p in o) {
        if (has(other, p))
            n++;
    }
    return n;
}
noInline(crossCheck);

function deleteDuring(o, victim) {
    const seen = [];
    for (const p in o) {
        if (p === 'a')
            delete o[victim];
        seen.push(p + ':' + has(o, p));
    }
    return seen.join(',');
}
noInline(deleteDuring);

function countIndexed(o) {
    let n = 0;
    for (const p in o) {
        if (Reflect.has(o, p))
            n++;
    }
    return n;
}
noInline(countIndexed);

const proto = { pa: 1, pb: 2 };
const shadowProto = { a: 42 };

for (let i = 0; i < testLoopCount; ++i) {
    const o = Object.create(proto);
    o.a = 1;
    o.b = 2;
    o.c = 3;
    // for-in visits own a, b, c and inherited pa, pb; `in` is true for all of them.
    assert(countIn(o) === 5, "countIn " + countIn(o));
    assert(countInHelper(o) === 5, "countInHelper");

    const shadow = Object.create(proto);
    shadow.pa = 9;
    shadow.x = 1;
    // Visits own pa (shadowing), x, and inherited pb.
    assert(countIn(shadow) === 3, "shadow " + countIn(shadow));

    assert(crossCheck({ a: 1, b: 2, c: 3 }, { a: 1, c: 3 }) === 2, "cross");

    const d = { a: 1, b: 2, c: 3 };
    const r = deleteDuring(d, 'c');
    assert(r === 'a:true,b:true' || r === 'a:true,b:true,c:false', "delete " + r);

    // Deleting the own property must still find it on the prototype chain.
    const s = Object.create(shadowProto);
    s.a = 1;
    s.b = 2;
    const rs = deleteDuring(s, 'a');
    assert(rs === 'a:true,b:true' || rs === 'a:true,b:true,a:true', "delete shadow " + rs);

    const arr = [10, 20, 30];
    arr.named = 1;
    assert(countIndexed(arr) === 4, "indexed");
}
