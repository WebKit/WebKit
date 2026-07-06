function assert(b, m) {
    if (!b)
        throw new Error(m);
}

const hop = Object.prototype.hasOwnProperty;

function countOwn(o) {
    let own = 0;
    let inherited = 0;
    for (const p in o) {
        if (Object.prototype.hasOwnProperty.call(o, p))
            own++;
        else
            inherited++;
    }
    return [own, inherited];
}
noInline(countOwn);

function countHasOwn(o) {
    let own = 0;
    for (const p in o) {
        if (Object.hasOwn(o, p))
            own++;
    }
    return own;
}
noInline(countHasOwn);

function crossCheck(o, other) {
    let n = 0;
    for (const p in o) {
        if (hop.call(other, p))
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
        seen.push(p + ':' + hop.call(o, p));
    }
    return seen.join(',');
}
noInline(deleteDuring);

function countIndexed(o) {
    let own = 0;
    for (const p in o) {
        if (hop.call(o, p))
            own++;
    }
    return own;
}
noInline(countIndexed);

const proto = { pa: 1, pb: 2 };

for (let i = 0; i < testLoopCount; ++i) {
    const o = Object.create(proto);
    o.a = 1;
    o.b = 2;
    o.c = 3;
    const [own, inherited] = countOwn(o);
    assert(own === 3, "own " + own);
    assert(inherited === 2, "inherited " + inherited);

    const shadow = Object.create(proto);
    shadow.pa = 9;
    shadow.x = 1;
    const [sOwn, sInherited] = countOwn(shadow);
    assert(sOwn === 2, "shadow own " + sOwn);
    assert(sInherited === 1, "shadow inherited " + sInherited);

    const h = Object.create(proto);
    h.a = 1;
    h.b = 2;
    assert(countHasOwn(h) === 2, "hasOwn");

    assert(crossCheck({ a: 1, b: 2, c: 3 }, { a: 1, c: 3 }) === 2, "cross");

    const d = { a: 1, b: 2, c: 3 };
    const r = deleteDuring(d, 'c');
    assert(r === 'a:true,b:true' || r === 'a:true,b:true,c:false', "delete " + r);

    const arr = [10, 20, 30];
    arr.named = 1;
    assert(countIndexed(arr) === 4, "indexed");
}
