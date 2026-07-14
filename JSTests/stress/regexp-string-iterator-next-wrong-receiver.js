const regExpIteratorProto = Object.getPrototypeOf("aa".matchAll(/a/g));
const regExpIteratorNext = regExpIteratorProto.next;

function sink(a, b, c, d, e, f) {
    return a || b || c || d || e || f;
}

function makeRegExpIterator(i) {
    const subject = (i & 1) ? "ababa" : "aaaa";
    const pattern = (i & 2) ? /a/g : /(?:a)/g;
    return subject.matchAll(pattern);
}

function makeWrongIterator(i) {
    const array = [i, i + 1, i + 2, i + 3];
    if (i & 4)
        array.extra = "shape";
    return array[Symbol.iterator]();
}

function probe(i) {
    let iter;
    const spreadIter = makeRegExpIterator(i);
    const proto = Object.getPrototypeOf(spreadIter);
    try {
        sink(...spreadIter, ...proto);
    } catch (e) {
    }
    if (i & 1)
        iter = makeWrongIterator(i);
    else
        iter = makeRegExpIterator(i);
    if (i & 2)
        iter = Object.getPrototypeOf(iter);
    if (i & 4)
        iter = [iter, i + 1, i + 2, i + 3][Symbol.iterator]();
    try {
        return regExpIteratorNext.call(iter);
    } catch (e) {
        return e;
    }
}

for (let i = 0; i < testLoopCount; ++i)
    probe(i);
