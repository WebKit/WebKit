// A non-registered Symbol stays reachable while it is a property key of a live
// object, so WeakMap, WeakSet and WeakRef must keep it, even when no variable
// references the Symbol value itself. A property key of a dead object must not
// keep it alive.

function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

const count = 200;

function makeHolders(addKey) {
    const holders = [];
    for (let i = 0; i < count; ++i) {
        const key = Symbol("key" + i);
        addKey(key, i);
        holders.push({ [key]: i });
    }
    return holders;
}

function keyOf(holder) {
    const keys = Object.getOwnPropertySymbols(holder);
    assert(keys.length === 1, "holder should have one symbol key");
    return keys[0];
}

const weakMap = new WeakMap();
const mapHolders = makeHolders((key, i) => weakMap.set(key, i));

const weakSet = new WeakSet();
const setHolders = makeHolders((key) => weakSet.add(key));

const weakRefs = [];
const refHolders = makeHolders((key) => weakRefs.push(new WeakRef(key)));

// Keys added in place to an old dictionary Structure, collected by an eden GC next.
const dictionary = {};
for (let i = 0; i < count; ++i)
    dictionary["p" + i] = i;
delete dictionary.p0;
const dictionaryMap = new WeakMap();
fullGC();
(function () {
    for (let i = 0; i < count; ++i) {
        const key = Symbol("dictionary" + i);
        dictionaryMap.set(key, i);
        dictionary[key] = i;
    }
})();
edenGC();
for (const key of Object.getOwnPropertySymbols(dictionary))
    assert(dictionaryMap.get(key) === dictionary[key], "WeakMap lost a dictionary symbol key");

// Each value holds its own key as a property and nothing else references either,
// so the pairs are garbage.
const cycleMap = new WeakMap();
const cycleRefs = [];
(function () {
    for (let i = 0; i < count; ++i) {
        const key = Symbol("cycle" + i);
        const value = { [key]: i };
        cycleMap.set(key, value);
        cycleRefs.push(new WeakRef(value));
    }
})();

function check(label) {
    for (let i = 0; i < count; ++i) {
        const key = keyOf(mapHolders[i]);
        assert(weakMap.has(key), `${label}: WeakMap lost entry ${i}`);
        assert(weakMap.get(key) === i, `${label}: WeakMap value ${i} changed`);
        assert(weakSet.has(keyOf(setHolders[i])), `${label}: WeakSet lost entry ${i}`);
    }
}

edenGC();
check("after eden GC");
fullGC();
check("after full GC");

// WeakRef targets are kept alive until the end of the current job, so collect
// again in a later task before checking them.
setTimeout(() => {
    fullGC();
    fullGC();
    check("after GC in a later task");
    for (let i = 0; i < count; ++i)
        assert(weakRefs[i].deref() === keyOf(refHolders[i]), `WeakRef ${i} lost its symbol`);

    // Conservative scanning may keep a few pairs alive; a leak keeps all of them.
    const alive = cycleRefs.filter((ref) => ref.deref() !== undefined).length;
    assert(alive < count / 2, `${alive} of ${count} unreachable WeakMap pairs were not collected`);
}, 0);
