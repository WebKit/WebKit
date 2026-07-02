function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

const error = new Error("boom");
const mapIteratorPrototype = Object.getPrototypeOf(new Map()[Symbol.iterator]());
const setIteratorPrototype = Object.getPrototypeOf(new Set()[Symbol.iterator]());

// Iterator.prototype.forEach over a Map iterator: "return" installed on the iterator
// mid-iteration must be invoked by IteratorClose when the callback throws.
{
    let returnCalled = 0;
    let resumeValue = null;
    const map = new Map([[1, 1], [2, 2], [3, 3]]);
    const iterator = map.keys();
    let caught = null;
    try {
        iterator.forEach(function (value) {
            if (value === 2) {
                iterator.return = function () {
                    returnCalled++;
                    resumeValue = this.next().value;
                    return { done: true };
                };
                throw error;
            }
        });
    } catch (e) {
        caught = e;
    }
    shouldBe(caught, error);
    shouldBe(returnCalled, 1);
    shouldBe(resumeValue, 3);
}

// Same for a Set iterator.
{
    let returnCalled = 0;
    let resumeValue = null;
    const set = new Set([1, 2, 3]);
    const iterator = set.values();
    let caught = null;
    try {
        iterator.forEach(function (value) {
            if (value === 2) {
                iterator.return = function () {
                    returnCalled++;
                    resumeValue = this.next().value;
                    return { done: true };
                };
                throw error;
            }
        });
    } catch (e) {
        caught = e;
    }
    shouldBe(caught, error);
    shouldBe(returnCalled, 1);
    shouldBe(resumeValue, 3);
}

// "return" installed on %MapIteratorPrototype% mid-iteration.
{
    let returnCalled = 0;
    const map = new Map([[1, 10], [2, 20], [3, 30]]);
    const iterator = map.entries();
    let caught = null;
    try {
        iterator.forEach(function (entry) {
            if (entry[0] === 2) {
                mapIteratorPrototype.return = function () {
                    returnCalled++;
                    return { done: true };
                };
                throw error;
            }
        });
    } catch (e) {
        caught = e;
    }
    delete mapIteratorPrototype.return;
    shouldBe(caught, error);
    shouldBe(returnCalled, 1);
}

// "return" installed on %SetIteratorPrototype% mid-iteration.
{
    let returnCalled = 0;
    const set = new Set([1, 2, 3]);
    const iterator = set.keys();
    let caught = null;
    try {
        iterator.forEach(function (value) {
            if (value === 2) {
                setIteratorPrototype.return = function () {
                    returnCalled++;
                    return { done: true };
                };
                throw error;
            }
        });
    } catch (e) {
        caught = e;
    }
    delete setIteratorPrototype.return;
    shouldBe(caught, error);
    shouldBe(returnCalled, 1);
}

// new Map(map) storage fast path: Map.prototype.set throwing mid-iteration must
// trigger IteratorClose, invoking a "return" installed on %MapIteratorPrototype%,
// with the iterator positioned where iteration stopped.
{
    let returnCalled = 0;
    let resumeKey = null;
    const map = new Map([[1, 10], [2, 20], [3, 30]]);
    const originalSet = Map.prototype.set;
    Map.prototype.set = function (key, value) {
        if (key === 2) {
            mapIteratorPrototype.return = function () {
                returnCalled++;
                resumeKey = this.next().value[0];
                return { done: true };
            };
            throw error;
        }
        return originalSet.call(this, key, value);
    };
    let caught = null;
    try {
        new Map(map);
    } catch (e) {
        caught = e;
    }
    Map.prototype.set = originalSet;
    delete mapIteratorPrototype.return;
    shouldBe(caught, error);
    shouldBe(returnCalled, 1);
    shouldBe(resumeKey, 3);
}

// new Set(set) storage fast path: same via Set.prototype.add.
{
    let returnCalled = 0;
    let resumeValue = null;
    const set = new Set([1, 2, 3]);
    const originalAdd = Set.prototype.add;
    Set.prototype.add = function (value) {
        if (value === 2) {
            setIteratorPrototype.return = function () {
                returnCalled++;
                resumeValue = this.next().value;
                return { done: true };
            };
            throw error;
        }
        return originalAdd.call(this, value);
    };
    let caught = null;
    try {
        new Set(set);
    } catch (e) {
        caught = e;
    }
    Set.prototype.add = originalAdd;
    delete setIteratorPrototype.return;
    shouldBe(caught, error);
    shouldBe(returnCalled, 1);
    shouldBe(resumeValue, 3);
}

// Normal exhaustion must not invoke "return".
{
    let returnCalled = 0;
    const map = new Map([[1, 1], [2, 2]]);
    const iterator = map.keys();
    iterator.return = function () {
        returnCalled++;
        return { done: true };
    };
    iterator.forEach(function () { });
    shouldBe(returnCalled, 0);
}
