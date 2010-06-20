description("Test for orphans");

function test()
{
    createBlockWithRatioToPageHeight("page1-1", 0.5).style.pageBreakBefore = "always";
    // Here about 10 lines are avaliable at the bottom of the current page.
    createBlockWithNumberOfLines("page1-2", 15).style.orphans = 8;

    createBlockWithRatioToPageHeight("page3", 0.5).style.pageBreakBefore = "always";
    // we should put 'page4' block in the next page because of orphans.
    createBlockWithNumberOfLines("page4", 15).style.orphans = 12;

    createBlockWithRatioToPageHeight("page5-1", 0.5).style.pageBreakBefore = "always";
    // '-1' is acceptable as a value of orphans. But it should have no effect.
    createBlockWithNumberOfLines("page5-2", 20).style.orphans = -1;

    pageNumberForElementShouldBe("page1-1", 1);
    pageNumberForElementShouldBe("page1-2", 1);

    pageNumberForElementShouldBe("page3", 3);
    pageNumberForElementShouldBe("page4", 4);

    pageNumberForElementShouldBe("page5-1", 5);
    pageNumberForElementShouldBe("page5-2", 5);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
