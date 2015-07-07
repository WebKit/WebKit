description("Test for page-break-inside:avoid");

function test()
{
    createBlockWithRatioToPageHeight("page1-1", 0.5).style.pageBreakBefore = "always";
    // block 'page1-2' should be splitted.
    createBlockWithNumberOfLines("page1-2", 20);
    createBlockWithRatioToPageHeight("page2", 0.1);

    createBlockWithRatioToPageHeight("page3", 0.5).style.pageBreakBefore = "always";
    // We should place block 'page4' in the next page because of 'page-break-inside: avoid'.
    createBlockWithNumberOfLines("page4", 20).style.pageBreakInside = "avoid";

    createBlockWithRatioToPageHeight("page5", 0.8).style.pageBreakBefore = "always";
    // block 'page5-2' is a very large block, occupying more than 2 pages.
    // We don't define from where this large block starts. Maybe from the next page.
    createBlockWithNumberOfLines("page5or6", 20 * 2 + 10).style.pageBreakInside = "avoid";
    createBlockWithRatioToPageHeight("page8", 0.1);

    createBlockWithRatioToPageHeight("page9-1", 0.1).style.pageBreakBefore = "always";
    createBlockWithRatioToPageHeight("page9-2", 0.1).style.pageBreakAfter = "always";
    // Make sure page-break only happens once, not twice.
    createBlockWithNumberOfLines("page10", 20).style.pageBreakInside = "avoid";

    pageNumberForElementShouldBe('page1-1', 1);
    pageNumberForElementShouldBe('page1-2', 1);
    pageNumberForElementShouldBe('page2', 2);

    pageNumberForElementShouldBe('page3', 3);
    pageNumberForElementShouldBe('page4', 4);

    pageNumberForElementShouldBe('page5', 5);
    pageNumberForElementShouldBe('page8', 8);

    pageNumberForElementShouldBe('page9-1', 9);
    pageNumberForElementShouldBe('page9-2', 9);
    pageNumberForElementShouldBe('page10', 10);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
