description("Tests find going both forward and backwards in small and large documents.");

var selection = getSelection();

function testFind(subjectString, pattern, backwards)
{
    var textNode = document.createTextNode(subjectString);
    document.body.appendChild(textNode);
    selection.removeAllRanges();
    if (backwards) {
        var afterTextNodeRange = document.createRange();
        afterTextNodeRange.setStartAfter(textNode);
        afterTextNodeRange.setEndAfter(textNode);
        selection.addRange(afterTextNodeRange);
    } else {
        var beforeTextNodeRange = document.createRange();
        beforeTextNodeRange.setStartBefore(textNode);
        beforeTextNodeRange.setEndBefore(textNode);
        selection.addRange(beforeTextNodeRange);
    }
    var result;
    if (!find(pattern, false, backwards))
        result = "not found"
    else if (selection.rangeCount != 1)
        result = "internal inconsistency";
    else {
        var resultRange = selection.getRangeAt(0);
        if (resultRange.startContainer !== textNode || resultRange.endContainer !== textNode)
            result = "not found";
        else
            result = resultRange.startOffset + ", " + resultRange.endOffset;
    }
    document.body.removeChild(textNode);
    return result;
}

var forward = false;
var backward = true;

var manyCharacters = "1234567890"
for (i = 0; i < 10; ++i)
    manyCharacters += manyCharacters;
var tenThousandCharacters = manyCharacters.substring(0, 10000);

shouldBe("testFind('abc', 'a', forward)", "'0, 1'");
shouldBe("testFind('abc', 'b', forward)", "'1, 2'");
shouldBe("testFind('abc', 'c', forward)", "'2, 3'");
shouldBe("testFind('abc', 'a', backward)", "'0, 1'");
shouldBe("testFind('abc', 'b', backward)", "'1, 2'");
shouldBe("testFind('abc', 'c', backward)", "'2, 3'");
shouldBe("testFind(tenThousandCharacters + 'abc' + tenThousandCharacters, 'a', forward)", "'10000, 10001'");
shouldBe("testFind(tenThousandCharacters + 'abc' + tenThousandCharacters, 'b', forward)", "'10001, 10002'");
shouldBe("testFind(tenThousandCharacters + 'abc' + tenThousandCharacters, 'c', forward)", "'10002, 10003'");
shouldBe("testFind(tenThousandCharacters + 'abc' + tenThousandCharacters, 'a', backward)", "'10000, 10001'");
shouldBe("testFind(tenThousandCharacters + 'abc' + tenThousandCharacters, 'b', backward)", "'10001, 10002'");
shouldBe("testFind(tenThousandCharacters + 'abc' + tenThousandCharacters, 'c', backward)", "'10002, 10003'");
shouldBe("testFind('abcabc', 'a', forward)", "'0, 1'");
shouldBe("testFind('abcabc', 'b', forward)", "'1, 2'");
shouldBe("testFind('abcabc', 'c', forward)", "'2, 3'");
shouldBe("testFind('abcabc', 'a', backward)", "'3, 4'");
shouldBe("testFind('abcabc', 'b', backward)", "'4, 5'");
shouldBe("testFind('abcabc', 'c', backward)", "'5, 6'");
shouldBe("testFind(tenThousandCharacters + 'abcabc' + tenThousandCharacters, 'a', forward)", "'10000, 10001'");
shouldBe("testFind(tenThousandCharacters + 'abcabc' + tenThousandCharacters, 'b', forward)", "'10001, 10002'");
shouldBe("testFind(tenThousandCharacters + 'abcabc' + tenThousandCharacters, 'c', forward)", "'10002, 10003'");
shouldBe("testFind(tenThousandCharacters + 'abcabc' + tenThousandCharacters, 'a', backward)", "'10003, 10004'");
shouldBe("testFind(tenThousandCharacters + 'abcabc' + tenThousandCharacters, 'b', backward)", "'10004, 10005'");
shouldBe("testFind(tenThousandCharacters + 'abcabc' + tenThousandCharacters, 'c', backward)", "'10005, 10006'");

var successfullyParsed = true;
