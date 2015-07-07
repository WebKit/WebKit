description("Test for no-content empty pages");

function test()
{
    // Only one page break should happen at the same position.
    createBlockWithRatioToPageHeight("page1-1", 0).style.pageBreakBefore = "always";
    createBlockWithRatioToPageHeight("page1-2", 0).style.pageBreakBefore = "always";
    createBlockWithRatioToPageHeight("page1-3", 0).style.pageBreakBefore = "always";
    createBlockWithRatioToPageHeight("page1-last", 0.1);

    pageNumberForElementShouldBe("page1-1", 1);
    pageNumberForElementShouldBe("page1-last", 1);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
