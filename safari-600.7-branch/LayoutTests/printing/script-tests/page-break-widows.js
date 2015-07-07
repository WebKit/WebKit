description("Test for widows");

function test()
{
    createBlockWithRatioToPageHeight("page1-1", 0.5).style.pageBreakBefore = "always";
    // Here about 10 lines are avaliable at the bottom of the current page.
    // We can leave about 5 lines in the next page. It doesn't violate widows restriction.
    createBlockWithNumberOfLines("page1-2", 15).style.widows = 2;
    // Here about 15 lines should be available in the current page.
    createBlockWithNumberOfLines("page2-1", 10);
    createBlockWithNumberOfLines("page2-2", 2);

    createBlockWithRatioToPageHeight("page3-1", 0.5).style.pageBreakBefore = "always";
    // The second part of 'page3-2' block must contain at least 12 lines in the next page.
    // So the block must be separated in earlier position.
    createBlockWithNumberOfLines("page3-2", 15).style.widows = 12;
    // Here at most 8 (= 20 - 12) lines should be available.
    createBlockWithNumberOfLines("page4", 10);
    createBlockWithNumberOfLines("page5", 2);

    pageNumberForElementShouldBe("page1-1", 1);
    pageNumberForElementShouldBe("page1-2", 1);
    pageNumberForElementShouldBe("page2-1", 2);
    pageNumberForElementShouldBe("page2-2", 2);

    pageNumberForElementShouldBe("page3-1", 3);
    pageNumberForElementShouldBe("page3-2", 3);
    pageNumberForElementShouldBe("page4", 4);
    pageNumberForElementShouldBe("page5", 5);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
