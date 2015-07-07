description("Test that the height of elements whose height depends on the viewport height is computed relative to the page height when printing.");

function test()
{
    numberOfPagesShouldBe(1);
    numberOfPagesShouldBe(1, 1000, 100);
    numberOfPagesShouldBe(1, 1000, 10000);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
