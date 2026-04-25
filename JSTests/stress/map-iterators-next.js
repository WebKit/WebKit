// This test checks the behavior of the iterator.next methods on Map objects

var testArray = [1,2,3,4,5,6]
var testMap = new Map();
for (var [key, value] of testArray.entries()) {
    testMap.set(key, value);
}
var keys = testMap.keys();
var i = 0;
while (true) {
    var {done, value: key} = keys.next();
    if (done)
        break;
    if (key >= testArray.length)
        throw "Error: bad value: " + key;
    i++;
}

if (testMap.size !== i)
    throw "Error: bad value: " + i;

var value = keys.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var values = testMap.values();
var i = 0;
while (true) {
    var {done, value} = values.next();
    if (done)
        break;
    i++;
    if (testArray.indexOf(value) === -1)
        throw "Error: bad value: " + value;
}

if (testMap.size !== i)
    throw "Error: bad value: " + i;

var value = values.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var entries = testMap.entries();
var i = 0;
do {
    var {done, value: entry} = entries.next();
    if (done)
        break;
    var [key, value] = entry;
    if (value !== testMap.get(key))
        throw "Error: bad value: " + value + " " + testMap.get(key);
    if (key >= testArray.length)
        throw "Error: bad value: " + key;
    i++;
    if (testArray.indexOf(value) === -1)
        throw "Error: bad value: " + value + " " + i;
} while (!done);

if (testMap.size !== i)
    throw "Error: bad value: " + i;

var value = entries.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var entries = testMap.entries();
var i = 0;
do {
    var {done, value: entry} = entries.next();
    if (done)
        break;
    var [key, value] = entry;
    if (value !== testMap.get(key))
        throw "Error: bad value: " + value + " " + testMap.get(key);
    i++;
    if (i % 4 === 0)
        testMap.set(100000 + i, i);
} while (!done);

if (testMap.size !== i)
    throw "Error: bad value: " + i;

var value = entries.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

function otherKey(key) {
    return (key + 1) % testArray.length;
}

var entries = testMap.entries();
var i = 0;
do {
    var {done, value: entry} = entries.next();
    if (done)
        break;
    var [key, value] = entry;
    if (value !== testMap.get(key))
        throw "Error: bad value: " + value + " " + testMap.get(key);
    i++;
    if (i % 4 === 0)
        testMap.delete(otherKey(key));
} while (!done);

if (testMap.size !== i)
    throw "Error: bad value: " + i;

var value = entries.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;
