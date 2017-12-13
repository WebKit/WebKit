function assert(b) {
    if (!b)
        throw new Error("Bad!");
}
noInline(assert);

let weakSet = new WeakSet;

function objectKey(o) {
    return weakSet.has(o);
}
noInline(objectKey);

const iters = 1000000;
let start = Date.now();

{
    let o = {f: 20};
    var array = [];
    for (var i = 0; i < 10; i++) {
        let newObject = { f: i };
        weakSet.add(newObject);
        array[i] = newObject;
    }

    for (var j = 0; j < iters; ++j) {
        for (let i = 0; i < 10; i++)
            assert(objectKey(array[i]) === true);
    }
}

const verbose = false;
if (verbose)
    print(Date.now() - start);
