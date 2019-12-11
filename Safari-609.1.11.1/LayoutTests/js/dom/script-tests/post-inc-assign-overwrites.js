description(
'Test for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=91270">'
);

function postIncDotAssignToBase()
{
    var obj = { property: 0 };
    var base = obj;

    base = base.property++;

    return obj.property === 1;
}

function postIncBracketAssignToBase()
{
    var obj = { property: 0 };
    var base = obj;
    var subscript = "property";

    base = base[subscript]++;

    return obj.property === 1;
}

function postIncBracketAssignToSubscript()
{
    var obj = { property: 0 };
    var base = obj;
    var subscript = "property";

    subscript = base[subscript]++;

    return obj.property === 1;
}

shouldBeTrue('postIncDotAssignToBase()');
shouldBeTrue('postIncBracketAssignToBase()');
shouldBeTrue('postIncBracketAssignToSubscript()');

successfullyParsed = true;
