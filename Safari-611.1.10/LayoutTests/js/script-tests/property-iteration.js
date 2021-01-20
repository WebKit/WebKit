description("This tests ordering of static properties.");

function arrayEqual(a,b)
{
    if (a.length != b.length)
        return false;
    for (var i in a)
        if (a[i] !== b[i])
            return false;
    return true;
}

// Assigning to 'Math.sin' should not change iteration order of properties.
var before = Object.getOwnPropertyNames(Math);
Math.sin = 42;
var after = Object.getOwnPropertyNames(Math);

shouldBeTrue('arrayEqual(before, after)');

successfullyParsed = true;
