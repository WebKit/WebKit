function testFunction() {
    if (arguments.length !== 10)
        throw "wrong number of arguments expected 10 was " + arguments.length;
    for (let i in arguments) {
        if ((arguments[i] | 0) !== (i | 0))
            throw "argument " + i + " expected " + i + " was " + arguments[i];
    }
}

function testEmpty() {
    if (arguments.length !== 0)
        throw "wrong length expected 0 was " + arguments.length;
}

iter = Array.prototype.values;

function makeObject(array, iterator) {
    let obj = { [Symbol.iterator]: iterator, length: array.length };
    for (let i in array)
        obj[i] = array[i];
    return obj;
}

function otherIterator() {
    return {
        count: 6,
        next: function() {
            if (this.count < 10)
                return { value: this.count++, done: false };
            return { done: true };
        }
    };
}

count = 0;
function* totalIter() {
    for (let i = count; i < count+5; i++) {
        yield i;
    }
    count += 5;
}

function throwingIter() {
     return {
        count: 0,
        next: function() {
            if (this.count < 10)
                return { value: this.count++, done: false };
            throw new Error("this should have been caught");
        }
    };
}

object1 = makeObject([1, 2, 3], iter);
object2 = makeObject([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], iter);
object3 = makeObject([], otherIterator);
object4 = makeObject([], totalIter);
objectThrow = makeObject([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], throwingIter);

for (let i = 0; i < 10000; i++) {
    count = 0;
    testFunction(0, ...[1, 2, 3], ...[4], 5, 6, ...[7, 8, 9]);
    testFunction(...[0, 1], 2, 3, ...[4, 5, 6, 7, 8], 9);
    testFunction(...[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]);
    testFunction(0, ...object1, 4, 5, ...[6, 7, 8, 9]);
    testFunction(...object2);
    testFunction(0, ...object1, 4, 5, ...object3);
    testFunction(0, ..."12345", ...object3);
    testEmpty(...[]);
    testFunction(...object4, ...object4);
    let failed = false;
    try {
        testFunction(...objectThrow);
        failed = true;
    } catch (e) {
        if (!e instanceof Error)
            failed = true;
    }
    if (failed)
        throw "did not throw an exeption even though it should have";
}
