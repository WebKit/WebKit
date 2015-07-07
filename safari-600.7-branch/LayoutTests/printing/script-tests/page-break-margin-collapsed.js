description("Test for big margin.");

function test()
{
    createBlockWithRatioToPageHeight("page1", 0.1).style.pageBreakBefore = "always";
    createBlockWithRatioToPageHeight("page2", 0.1).style.marginTop = "100000px";

    pageNumberForElementShouldBe("page1", 1);
    // Instead of inserting many empty pages, we should place block 'page2'
    // on the next page, collapsing the margin between vetically adjacent blocks.
    pageNumberForElementShouldBe("page2", 2);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
