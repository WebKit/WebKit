description("Test to make sure 2nd page is printed.");

function test()
{
    createBlockWithRatioToPageHeight("page1", 1);
    createBlockWithRatioToPageHeight("page2", 0.5);

    pageNumberForElementShouldBe("page1", 0);
    pageNumberForElementShouldBe("page2", 1);
    
    numberOfPagesShouldBe(2);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
