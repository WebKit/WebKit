description("Test for page-break with 'display:none'");

function test()
{
    createBlockWithRatioToPageHeight("page1-1", 0.1).style.pageBreakBefore = "always";

    // if 'display' is 'none', page break property should not have any effect.
    var block = createBlockWithRatioToPageHeight("displaynone", 0.1);
    block.style.pageBreakBefore = "always";
    block.style.display = "none";

    createBlockWithRatioToPageHeight("page1-2", 0.1);

    pageNumberForElementShouldBe("page1-1", 1);
    pageNumberForElementShouldBe("displaynone", -1);
    pageNumberForElementShouldBe("page1-2", 1);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
