description("Test for combined page-break-{before,after,inside}:avoid");

function test()
{
    var block;
    createBlockWithRatioToPageHeight("page1-1", 0.4).style.breakBefore = "page";
    // A block 'page2-1' must move to the next page because we cannot find any
    // allowed page breaks till the end of block page2-8.
    createBlockWithRatioToPageHeight("page2-1", 0.1).style.breakAfter = "avoid-page";
    createBlockWithRatioToPageHeight("page2-2", 0.1);
    createBlockWithRatioToPageHeight("page2-3", 0.1).style.breakBefore = "avoid-page";
    createBlockWithRatioToPageHeight("page2-4", 0.1).style.breakBefore = "avoid-page";

    block = createBlockWithRatioToPageHeight("page2-5", 0.1);
    block.style.breakBefore = "avoid-page";
    block.style.breakAfter = "avoid-page";

    createBlockWithRatioToPageHeight("page2-6", 0.1);

    block = createBlockWithNumberOfLines("page2-7", 4);
    block.style.breakBefore = "avoid-page";
    block.style.breakAfter = "avoid-page";
    block.style.breakInside = "avoid-page";

    createBlockWithRatioToPageHeight("page2-8", 0.1).style.breakBefore = "avoid-page";

    pageNumberForElementShouldBe("page1-1", 1);
    pageNumberForElementShouldBe("page2-1", 2);
    // Omit tests for intermediate blocks.
    pageNumberForElementShouldBe("page2-8", 2);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
