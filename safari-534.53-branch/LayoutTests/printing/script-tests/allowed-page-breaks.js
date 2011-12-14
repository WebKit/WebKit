description("Test for allowed page breaks");

function test()
{
    // See CSS3: Paged Media 9.4. Allowed page breaks.
    // http://dev.w3.org/csswg/css3-page/#allowed-pg-brk
    createBlockWithRatioToPageHeight("page1", 0.8).style.pageBreakBefore = "always";
    // We shoud break a page between block 'page1' and block 'page2' instead of
    // breaking inside block 'page2' because there is no allowed page break points
    // inside block 'page2'. Breaking at non-allowed position should be a last resort.
    createBlockWithRatioToPageHeight("page2", 0.8);

    createBlockWithRatioToPageHeight("page3-1", 0.6).style.pageBreakBefore = "always";
    // We can break inside block 'page3-2' because page breaks may occur between
    // line boxes inside a block box.
    createBlockWithNumberOfLines("page3-2", 20);

    pageNumberForElementShouldBe("page1", 1);
    pageNumberForElementShouldBe("page2", 2);
    pageNumberForElementShouldBe("page3-1", 3);
    pageNumberForElementShouldBe("page3-2", 3);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
