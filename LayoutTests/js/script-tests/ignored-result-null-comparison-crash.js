description(
"This tests that bytecode generation doesn't crash on a comparison to null with an ignored result."
);

function equalToNullTest(a)
{
    a == null;
    return true;
}

shouldBeTrue("equalToNullTest()");

function notEqualToNullTest(a)
{
    a != null;
    return true;
}

shouldBeTrue("notEqualToNullTest()");
