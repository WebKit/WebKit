description("Test for page-break-before:avoid");

function test()
{
    createBlockWithRatioToPageHeight("page1", 0.5).style.pageBreakBefore = "always";
    // A block 'page2-1' must move to the next page because the following block 'page2-2'
    // has 'page-break-before:avoid' and both blocks cannot be placed in the current page
    // at the same time.
    createBlockWithRatioToPageHeight("page2-1", 0.3);
    createBlockWithRatioToPageHeight("page2-2", 0.3).style.pageBreakBefore = "avoid";

    createBlockWithRatioToPageHeight("page3-1", 0.5).style.pageBreakBefore = "always";
    // A page break can occur inside of 'page3-3' block because it has child elements.
    createBlockWithRatioToPageHeight("page3-2", 0.3);
    createBlockWithNumberOfLines("page3-3", 10).style.pageBreakBefore = "avoid";

    createBlockWithRatioToPageHeight("page5", 0.5).style.pageBreakBefore = "always";
    // It seems unrealistic, but block 'page6-1' must move to the next page.
    createBlockWithRatioToPageHeight("page6-1", 0.1);
    createBlockWithRatioToPageHeight("page6-2", 0.1).style.pageBreakBefore = "avoid";
    createBlockWithRatioToPageHeight("page6-3", 0.1).style.pageBreakBefore = "avoid";
    createBlockWithRatioToPageHeight("page6-4", 0.1).style.pageBreakBefore = "avoid";
    createBlockWithRatioToPageHeight("page6-5", 0.1).style.pageBreakBefore = "avoid";
    createBlockWithRatioToPageHeight("page6-6", 0.1).style.pageBreakBefore = "avoid";
    createBlockWithRatioToPageHeight("page6-7", 0.1).style.pageBreakBefore = "avoid";

    // We don't define where block 'page7or8-1' is placed.
    createBlockWithRatioToPageHeight("page7", 0.5).style.pageBreakBefore = "always";
    createBlockWithRatioToPageHeight("page7or8-1", 0.1);
    createBlockWithRatioToPageHeight("page7or8-2", 3).style.pageBreakBefore = "avoid";
    createBlockWithRatioToPageHeight("page10", 0.1);

    pageNumberForElementShouldBe("page1", 1);
    pageNumberForElementShouldBe("page2-1", 2);
    pageNumberForElementShouldBe("page2-2", 2);

    pageNumberForElementShouldBe("page3-1", 3);
    pageNumberForElementShouldBe("page3-2", 3);
    pageNumberForElementShouldBe("page3-3", 3);

    pageNumberForElementShouldBe("page5", 5);
    pageNumberForElementShouldBe("page6-1", 6);
    // Omit tests for intermediate blocks.
    pageNumberForElementShouldBe("page6-7", 6);

    pageNumberForElementShouldBe("page7", 7);
    pageNumberForElementShouldBe("page10", 10);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
