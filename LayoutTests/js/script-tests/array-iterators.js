description(
"This test checks the behavior of the iterator methods on Array objects."
);


var testArray = [1,2,3,4,5,6]
var keys = testArray.keys();
var i = 0;
while (true) {
    var {done, value: key} = keys.next();
    if (done)
        break;
    shouldBe("key", String(i))
    i++;
}

shouldBe("testArray.length", String(i))

shouldBeUndefined("keys.next().value")

var values = testArray.values();
var i = 0;
while (true) {
    var {done, value} = values.next();
    if (done)
        break;
    i++;
    shouldBe("value", String(i) )
}

shouldBe("testArray.length", String(i))

shouldBeUndefined("values.next().value")

var entries = testArray.entries();
var i = 0;
do {
    var {done, value: entry} = entries.next();
    if (done)
        break;
    var [key, value] = entry;
    shouldBe("value", "testArray[key]")
    shouldBe("key", String(i))
    i++
    shouldBe("value", String(i))
} while (!done);

shouldBe("testArray.length", String(i))

shouldBeUndefined("entries.next().value")



var entries = testArray.entries();
var i = 0;
do {
    var {done, value: entry} = entries.next();
    if (done)
        break;
    var [key, value] = entry;
    shouldBe("value", "testArray[key]")
    shouldBe("key", "i")
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
shouldBe("testArray.length", String(i))

shouldBeUndefined("entries.next().value")

