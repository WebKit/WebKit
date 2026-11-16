// This test checks the behavior of the iterator.next methods on Set objects

var testArray = [1,2,3,4,5,6]
var testSet = new Set();
for (var [key, value] of testArray.entries()) {
    testSet.add(value);
}
var keys = testSet.keys();
var i = 0;
while (true) {
    var {done, value: key} = keys.next();
    if (done)
        break;
    if (testArray.indexOf(key) === -1)
        throw "Error: bad value: " + key;
    i++;
}

if (testSet.size !== i)
    throw "Error: bad value: " + i;

var value = keys.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var values = testSet.values();
var i = 0;
while (true) {
    var {done, value} = values.next();
    if (done)
        break;
    i++;
    if (testArray.indexOf(value) === -1)
        throw "Error: bad value: " + value;
}

if (testSet.size !== i)
    throw "Error: bad value: " + i;

var value = values.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var entries = testSet.entries();
var i = 0;
do {
    var {done, value: entry} = entries.next();
    if (done)
        break;
    var [key, value] = entry;
    if (key !== value)
        throw "Error: bad value: " + key + " " + value;
    if (!testSet.has(value))
        throw "Error: bad value: " + value;
    if (!testSet.has(key))
        throw "Error: bad value: " + key;
    i++;
    if (testArray.indexOf(value) === -1)
        throw "Error: bad value: " + value + " " + i;
} while (!done);

if (testSet.size !== i)
    throw "Error: bad value: " + i;

var value = entries.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var entries = testSet.entries();
var i = 0;
do {
    var {done, value: entry} = entries.next();
    if (done)
        break;
    var [key, value] = entry;
    if (key !== value)
        throw "Error: bad value: " + key + " " + value;
    if (!testSet.has(key))
        throw "Error: bad value: " + value;
    i++;
    if (i % 4 === 0)
        testSet.add(100000 + i);
} while (!done);

if (testSet.size !== i)
    throw "Error: bad value: " + i;

var value = entries.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

function otherKey(key) {
    return (key + 1) % testArray.length;
}

var entries = testSet.entries();
var i = 0;
do {
    var {done, value: entry} = entries.next();
    if (done)
        break;
    var [key, value] = entry;
    if (!testSet.has(key))
        throw "Error: bad value: " + value + " " + testSet.get(key);
    i++;
    if (i % 4 === 0)
        testSet.delete(otherKey(key));
} while (!done);

if (testSet.size !== i)
    throw "Error: bad value: " + i;

var value = entries.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;
