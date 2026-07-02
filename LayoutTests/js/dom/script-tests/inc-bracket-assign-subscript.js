description(
'Test for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=95913">'
);

function testPreIncBracketAccessWithAssignSubscript()
{
    var x = { property: -1 };
    var y = { property: true };
    ++x[x = y, "property"];
    return x.property;
}

function testPostIncBracketAccessWithAssignSubscript()
{
    var x = { property: -1 };
    var y = { property: true };
    x[x = y, "property"]++;
    return x.property;
}

shouldBeTrue('testPreIncBracketAccessWithAssignSubscript()');
shouldBeTrue('testPostIncBracketAccessWithAssignSubscript()');

successfullyParsed = true;
