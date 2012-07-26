description("Test for internals.pageNumber()");

function test()
{
    // Assuming that one page has about 20 lines.
    createBlockWithNumberOfLines("firstPage", 20);
    createBlockWithNumberOfLines("secondPage", 20);

    createBlockWithNumberOfLines("thirdPage", 5);
    createBlockWithNumberOfLines("thirdPage2", 5);

    pageNumberForElementShouldBe('firstPage', 0);
    pageNumberForElementShouldBe('secondPage', 1);
    pageNumberForElementShouldBe('thirdPage', 2);
    pageNumberForElementShouldBe('thirdPage2', 2);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
