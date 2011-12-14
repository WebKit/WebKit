description("Test for orphans and widows");

function test()
{
    var block;
    createBlockWithNumberOfLines("page1-1", 10).style.pageBreakBefore = "always";
    // 10 lines lines are available in the current page.
    block = createBlockWithNumberOfLines("page1-2", 9);
    block.style.orphans = 10;
    block.style.widows = 20;
    // block 'page1-2' can be placed on the current page.
    createBlockWithRatioToPageHeight("page1-3", 0.01);

    createBlockWithNumberOfLines("page2", 10).style.pageBreakBefore = "always";
    // 10 lines lines are available in the current page.
    block = createBlockWithNumberOfLines("page3", 12);
    block.style.orphans = 15;
    block.style.widows = 2;
    // block 'page3' must move as a block to the next page because of 'orphans'.

    createBlockWithNumberOfLines("page4", 10).style.pageBreakBefore = "always";
    // 10 lines lines are available in the current page.
    block = createBlockWithNumberOfLines("page5", 12);
    block.style.orphans = 2;
    block.style.widows = 15;
    // block 'page5' must move as a block to the next page because of 'widows'.

    createBlockWithNumberOfLines("page6-1", 10).style.pageBreakBefore = "always";
    // 10 lines lines are available in the current page.
    block = createBlockWithNumberOfLines("page6-2", 12);
    block.style.orphans = 6;
    block.style.widows = 6;
    // block 'page6' must be splitted.
    createBlockWithRatioToPageHeight("page7", 0.01);

    createBlockWithNumberOfLines("page8", 10).style.pageBreakBefore = "always";
    // 10 lines lines are available in the current page.
    block = createBlockWithNumberOfLines("page9", 12);
    block.style.orphans = 10;
    block.style.widows = 10;
    // block 'page9' must move as a block to the next page because we cannot satisfy
    // both 'orphans' and 'widows' at the same time.

    pageNumberForElementShouldBe("page1-1", 1);
    pageNumberForElementShouldBe("page1-2", 1);
    pageNumberForElementShouldBe("page1-3", 1);

    pageNumberForElementShouldBe("page2", 2);
    pageNumberForElementShouldBe("page3", 3);

    pageNumberForElementShouldBe("page4", 4);
    pageNumberForElementShouldBe("page5", 5);

    pageNumberForElementShouldBe("page6-1", 6);
    pageNumberForElementShouldBe("page6-2", 6);
    pageNumberForElementShouldBe("page7", 7);

    pageNumberForElementShouldBe("page8", 8);
    pageNumberForElementShouldBe("page9", 9);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
