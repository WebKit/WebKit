// This test checks the behavior of the iterator.next methods on Array objects

var testArray = [1,2,3,4,5,6]
var keys = testArray.keys();
var i = 0;
while (true) {
    var {done, value: key} = keys.next();
    if (done)
        break;
    if (key !== i)
        throw "Error: bad value: " + key;
    i++;
}

if (testArray.length !== i)
    throw "Error: bad value: " + i;

var value = keys.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var values = testArray[Symbol.iterator]();
var i = 0;
while (true) {
    var {done, value} = values.next();
    if (done)
        break;
    i++;
    if (value !== i)
        throw "Error: bad value: " + value;
}

if (testArray.length !== i)
    throw "Error: bad value: " + i;

var value = values.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var values = testArray.values();
var i = 0;
while (true) {
    var {done, value} = values.next();
    if (done)
        break;
    i++;
    if (value !== i)
        throw "Error: bad value: " + value;
}

if (testArray.length !== i)
    throw "Error: bad value: " + i;

var value = values.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var entries = testArray.entries();
var i = 0;
do {
    var {done, value: entry} = entries.next();
    if (done)
        break;
    var [key, value] = entry;
    if (value !== testArray[key])
        throw "Error: bad value: " + value + " " + testArray[key];
    if (key !== i)
        throw "Error: bad value: " + key;
    i++
    if (value !== i)
        throw "Error: bad value: " + value + " " + i;
} while (!done);

if (testArray.length !== i)
    throw "Error: bad value: " + i;

var value = entries.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;

var entries = testArray.entries();
var i = 0;
do {
    var {done, value: entry} = entries.next();
    if (done)
        break;
    var [key, value] = entry;
    if (value !== testArray[key])
        throw "Error: bad value: " + value + " " + testArray[key];
    if (key !== i)
        throw "Error: bad value: " + key;
    i++
    if (i % 2 == 0)
        testArray[i] *= 2;
    if (i < 4)
        testArray.push(testArray.length)
    if (i == 4)
        delete testArray[4]
    if (i == 5)
        testArray[4] = 5
} while (!done);

if (testArray.length !== i)
    throw "Error: bad value: " + i;

var value = entries.next().value;
if (value !== undefined)
    throw "Error: bad value: " + value;
