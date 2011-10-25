description(
"This tests that a call to array.sort(compareFunction) works correctly for numeric comparisons (arg1 - arg2), and also for things that might look like numeric comparisons."
);

function doSort(x, y)
{
    return x - y;
}

function dontSort(w, x, y)
{
    return x - y;
}

shouldBe("[3,1,5,2,4].sort(doSort)", "[1,2,3,4,5]");
shouldBe("[3,1,5,2,4].sort(dontSort)", "[3,1,5,2,4]");
