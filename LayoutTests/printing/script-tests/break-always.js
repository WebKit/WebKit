description("Test for page-break-before:always and page-break-after:always");

function test()
{
    createBlockWithRatioToPageHeight("firstPage", 0.1);
    createBlockWithRatioToPageHeight("secondPage1", 0.1).style.breakBefore = "page";
    createBlockWithRatioToPageHeight("secondPage2", 0.1).style.breakAfter = "page";
    createBlockWithRatioToPageHeight("thirdPage", 0.1).style.breakBefore = "page";

    pageNumberForElementShouldBe('firstPage', 0);
    pageNumberForElementShouldBe('secondPage1', 1);
    pageNumberForElementShouldBe('secondPage2', 1);
    // There must be only one page break between 'page-break-after: always' and 'page-break-before: always'
    pageNumberForElementShouldBe('thirdPage', 2);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
