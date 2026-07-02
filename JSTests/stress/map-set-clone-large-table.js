function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

// 600 entries grow the backing table to capacity 2048, which is large enough for the bulk table copy.
const count = 600;

// Set cloning via new Set(set).
{
    const source = new Set();
    for (let i = 0; i < count; ++i)
        source.add(i);

    const clone = new Set(source);
    shouldBe(clone.size, count);
    let expected = 0;
    for (const value of clone)
        shouldBe(value, expected++);
    shouldBe(expected, count);

    // The clone is independent from the source.
    clone.add(count);
    clone.delete(0);
    shouldBe(source.size, count);
    shouldBe(source.has(0), true);
    shouldBe(source.has(count), false);

    // The clone keeps working as a Set (lookup, add, delete, rehash, GC).
    gc();
    for (let i = 0; i < count; ++i)
        shouldBe(clone.has(i), i !== 0);
    for (let i = 0; i < 2000; ++i)
        clone.add('extra' + i);
    shouldBe(clone.size, count + 2000);
    for (let i = 0; i < 2000; ++i)
        shouldBe(clone.delete('extra' + i), true);
    gc();
    shouldBe(clone.size, count);

    // Cloning a table with deleted entries takes the entry-by-entry path.
    source.delete(1);
    source.delete(599);
    const compacted = new Set(source);
    shouldBe(compacted.size, count - 2);
    shouldBe(compacted.has(1), false);
    shouldBe(compacted.has(599), false);
    shouldBe(compacted.has(2), true);
}

// Set cloning via Set.prototype.union with an empty-ish other set.
{
    const source = new Set();
    for (let i = 0; i < count; ++i)
        source.add('key' + i);

    const result = source.union(new Set([ 'key0' ]));
    shouldBe(result.size, count);
    let index = 0;
    for (const value of result)
        shouldBe(value, 'key' + index++);
    gc();
    shouldBe(result.has('key599'), true);
}

// Map cloning via new Map(map).
{
    const source = new Map();
    for (let i = 0; i < count; ++i)
        source.set('k' + i, i * 2);

    const clone = new Map(source);
    shouldBe(clone.size, count);
    let expected = 0;
    for (const [key, value] of clone) {
        shouldBe(key, 'k' + expected);
        shouldBe(value, expected * 2);
        ++expected;
    }
    shouldBe(expected, count);

    clone.set('k0', -1);
    clone.delete('k1');
    shouldBe(source.get('k0'), 0);
    shouldBe(source.has('k1'), true);

    gc();
    for (let i = 2; i < count; ++i)
        shouldBe(clone.get('k' + i), i * 2);

    source.delete('k0');
    const compacted = new Map(source);
    shouldBe(compacted.size, count - 1);
    shouldBe(compacted.has('k0'), false);
    shouldBe(compacted.get('k599'), 1198);
}
