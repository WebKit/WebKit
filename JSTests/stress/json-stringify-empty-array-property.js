function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + String(actual).slice(0, 200) + ' expected: ' + String(expected).slice(0, 200));
}

function reference(value, space) {
    return JSON.stringify(value, (key, value) => value, space);
}

function makeObjects(count, makeArray) {
    let objects = [];
    for (let i = 0; i < count; ++i)
        objects.push({ a: i, b: makeArray(i), c: [], d: 'x' });
    return objects;
}

for (let count of [1, 10, 1000]) {
    let objects = makeObjects(count, i => i % 3 ? [] : [i]);
    shouldBe(JSON.stringify(objects), reference(objects));
    shouldBe(JSON.stringify(objects, null, 1), reference(objects, 1));
}

{
    let withOwnToJSON = [];
    withOwnToJSON.toJSON = () => 'own';
    let objects = makeObjects(1000, i => i === 500 ? withOwnToJSON : []);
    shouldBe(JSON.stringify(objects), reference(objects));
    shouldBe(JSON.stringify(objects).includes('"b":"own"'), true);
}

{
    let withNamedProperty = [];
    withNamedProperty.extra = 1;
    let objects = makeObjects(1000, i => i % 7 ? [] : withNamedProperty);
    shouldBe(JSON.stringify(objects), reference(objects));
}

{
    let root = { };
    for (let i = 0; i < 1000; ++i)
        root['k' + i] = { a: i, b: [], c: 'x' };
    let expected = JSON.stringify(root);
    Array.prototype.toJSON = function () { return 'proto'; };
    let actual = JSON.stringify(root);
    let expectedWithToJSON = reference(root);
    delete Array.prototype.toJSON;
    shouldBe(actual, expectedWithToJSON);
    shouldBe(actual.includes('"b":"proto"'), true);
    shouldBe(JSON.stringify(root), expected);
}

{
    // Null-prototype root and holders, so only the nested arrays inherit Object.prototype.toJSON.
    let root = Object.create(null);
    for (let i = 0; i < 1000; ++i) {
        let holder = Object.create(null);
        holder.a = i;
        holder.b = i % 2 ? [] : [i];
        root['k' + i] = holder;
    }
    let expected = JSON.stringify(root);
    Object.prototype.toJSON = function () { return 'object-proto'; };
    let actual = JSON.stringify(root);
    let expectedWithToJSON = reference(root);
    delete Object.prototype.toJSON;
    shouldBe(actual, expectedWithToJSON);
    shouldBe(actual.includes('"b":"object-proto"'), true);
    shouldBe(actual.includes('[]'), false);
    shouldBe(JSON.stringify(root), expected);
}

{
    let makers = [
        () => { let a = [1, 2]; Object.defineProperty(a, 0, { value: 3, writable: false }); return a; },
        () => { let a = [1, 2, 3]; Object.defineProperty(a, 1, { get() { return 7; }, enumerable: true }); return a; },
        () => { let a = [1, 2, 3]; Object.preventExtensions(a); return a; },
        () => { let a = new Array(10); a[3] = 1; return a; },
    ];
    for (let maker of makers) {
        let objects = makeObjects(100, i => i % 5 ? [] : maker());
        shouldBe(JSON.stringify(objects), reference(objects));
        shouldBe(JSON.stringify(objects, null, 2), reference(objects, 2));
    }
}

{
    let otherRealm = createGlobalObject();
    class MyArray extends Array { }
    class MyArrayWithToJSON extends Array {
        toJSON() { return 'subclass'; }
    }
    let makers = [
        () => otherRealm.Array(),
        () => otherRealm.Array(1, 2),
        () => new MyArray(),
        () => MyArray.of(1, 2),
        () => new MyArrayWithToJSON(),
    ];
    for (let maker of makers) {
        let objects = makeObjects(500, i => i % 5 ? [] : maker());
        shouldBe(JSON.stringify(objects), reference(objects));
        shouldBe(JSON.stringify(objects, null, 2), reference(objects, 2));
    }
    otherRealm.Array.prototype.toJSON = function () { return 'other-realm'; };
    let objects = makeObjects(1000, i => i % 5 ? [] : otherRealm.Array());
    let actual = JSON.stringify(objects);
    shouldBe(actual, reference(objects));
    shouldBe(actual.includes('"b":"other-realm"'), true);
}

{
    let objects = [];
    for (let i = 0; i < 1000; ++i) {
        switch (i % 3) {
        case 0:
            objects.push({ a: i, b: [1, 'x', null], c: [] });
            break;
        case 1:
            objects.push({ a: i, b: [1, 2, 3], c: [] });
            break;
        case 2:
            objects.push({ a: i, b: [1.5, 2.5], c: [] });
            break;
        }
    }
    shouldBe(JSON.stringify(objects), reference(objects));
    shouldBe(JSON.stringify(objects, null, '\t'), reference(objects, '\t'));
}
