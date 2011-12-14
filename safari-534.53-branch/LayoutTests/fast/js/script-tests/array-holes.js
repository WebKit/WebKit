description("This tests that arrays have holes that you can see the prototype through, not just missing values.");

function isHole(array, index)
{
    if (index >= array.length)
        return "bad index: past length";
    // Check if we can see through the hole into another room.
    Array.prototype[index] = "room";
    var isHole = array[index] == "room";
    delete Array.prototype[index];
    return isHole;
}

function showHoles(array)
{
    var string = "[";
    for (i = 0; i < array.length; ++i) {
        if (i)
            string += ", ";
        if (isHole(array, i))
            string += "hole";
        else
            string += array[i];
    }
    string += "]";
    return string;
}

function returnTrue()
{
    return true;
}

var a;

function addToArray(arg)
{
    a.push(arg);
}

function addToArrayReturnFalse(arg)
{
    a.push(arg);
    return false;
}

function addToArrayReturnTrue(arg)
{
    a.push(arg);
    return true;
}

shouldBe("var a = []; a.length = 1; showHoles(a)", "'[hole]'");
shouldBe("var a = []; a[0] = undefined; showHoles(a)", "'[undefined]'");
shouldBe("var a = []; a[0] = undefined; delete a[0]; showHoles(a)", "'[hole]'");

shouldBe("showHoles([0, , 2])", "'[0, hole, 2]'");
shouldBe("showHoles([0, 1, ,])", "'[0, 1, hole]'");
shouldBe("showHoles([0, , 2].concat([3, , 5]))", "'[0, hole, 2, 3, hole, 5]'");
shouldBe("showHoles([0, , 2, 3].reverse())", "'[3, 2, hole, 0]'");
shouldBe("a = [0, , 2, 3]; a.shift(); showHoles(a)", "'[hole, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].slice(0, 3))", "'[0, hole, 2]'");
shouldBe("showHoles([0, , 2, 3].sort())", "'[0, 2, 3, hole]'");
shouldBe("showHoles([0, undefined, 2, 3].sort())", "'[0, 2, 3, undefined]'");
shouldBe("a = [0, , 2, 3]; a.splice(2, 3, 5, 6); showHoles(a)", "'[0, hole, 5, 6]'");
shouldBe("a = [0, , 2, 3]; a.unshift(4); showHoles(a)", "'[4, 0, hole, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].filter(returnTrue))", "'[0, 2, 3]'");
shouldBe("showHoles([0, undefined, 2, 3].filter(returnTrue))", "'[0, undefined, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].map(returnTrue))", "'[true, hole, true, true]'");
shouldBe("showHoles([0, undefined, 2, 3].map(returnTrue))", "'[true, true, true, true]'");
shouldBe("a = []; [0, , 2, 3].every(addToArrayReturnTrue); showHoles(a)", "'[0, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].every(addToArrayReturnTrue); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("a = []; [0, , 2, 3].forEach(addToArray); showHoles(a)", "'[0, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].forEach(addToArray); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("a = []; [0, , 2, 3].some(addToArrayReturnFalse); showHoles(a)", "'[0, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].some(addToArrayReturnFalse); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("[0, , 2, 3].indexOf()", "-1");
shouldBe("[0, undefined, 2, 3].indexOf()", "1");
shouldBe("[0, , 2, 3].lastIndexOf()", "-1");
shouldBe("[0, undefined, 2, 3].lastIndexOf()", "1");

Object.prototype[1] = "peekaboo";

shouldBe("showHoles([0, , 2])", "'[0, hole, 2]'");
shouldBe("showHoles([0, 1, ,])", "'[0, 1, hole]'");
shouldBe("showHoles([0, , 2].concat([3, , 5]))", "'[0, peekaboo, 2, 3, peekaboo, 5]'");
shouldBe("showHoles([0, , 2, 3].reverse())", "'[3, 2, peekaboo, 0]'");
shouldBe("a = [0, , 2, 3]; a.shift(); showHoles(a)", "'[peekaboo, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].slice(0, 3))", "'[0, peekaboo, 2]'");
shouldBe("showHoles([0, , 2, 3].sort())", "'[0, 2, 3, hole]'");
shouldBe("showHoles([0, undefined, 2, 3].sort())", "'[0, 2, 3, undefined]'");
shouldBe("a = [0, , 2, 3]; a.splice(2, 3, 5, 6); showHoles(a)", "'[0, hole, 5, 6]'");
shouldBe("a = [0, , 2, 3]; a.unshift(4); showHoles(a)", "'[4, 0, peekaboo, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].filter(returnTrue))", "'[0, peekaboo, 2, 3]'");
shouldBe("showHoles([0, undefined, 2, 3].filter(returnTrue))", "'[0, undefined, 2, 3]'");
shouldBe("showHoles([0, , 2, 3].map(returnTrue))", "'[true, true, true, true]'");
shouldBe("showHoles([0, undefined, 2, 3].map(returnTrue))", "'[true, true, true, true]'");
shouldBe("a = []; [0, , 2, 3].every(addToArrayReturnTrue); showHoles(a)", "'[0, peekaboo, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].every(addToArrayReturnTrue); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("a = []; [0, , 2, 3].forEach(addToArray); showHoles(a)", "'[0, peekaboo, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].forEach(addToArray); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("a = []; [0, , 2, 3].some(addToArrayReturnFalse); showHoles(a)", "'[0, peekaboo, 2, 3]'");
shouldBe("a = []; [0, undefined, 2, 3].some(addToArrayReturnFalse); showHoles(a)", "'[0, undefined, 2, 3]'");
shouldBe("[0, , 2, 3].indexOf()", "-1");
shouldBe("[0, , 2, 3].indexOf('peekaboo')", "1");
shouldBe("[0, undefined, 2, 3].indexOf()", "1");
shouldBe("[0, , 2, 3].lastIndexOf()", "-1");
shouldBe("[0, , 2, 3].lastIndexOf('peekaboo')", "1");
shouldBe("[0, undefined, 2, 3].lastIndexOf()", "1");

delete Object.prototype[1];

var successfullyParsed = true;
