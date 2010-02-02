description("Test for layoutTestController.pageNumberForElementById()");

function createParagraph(id)
{
    var element = document.createElement("div");
    element.id = id;
    element.width = 100;
    document.getElementById("sandbox").appendChild(element);
    return element;
}

var firstPage = createParagraph("firstPage");
firstPage.style.height = 1000;

var secondPage = createParagraph("secondPage");
secondPage.style.height = 1000;

var thirdPage = createParagraph("thirdPage");
thirdPage.style.height = 100;

var thirdPage2 = createParagraph("thirdPage2");
thirdPage2.style.height = 700;

shouldBe("layoutTestController.pageNumberForElementById('firstPage', 1000, 1000)", "0");
shouldBe("layoutTestController.pageNumberForElementById('secondPage', 1000, 1000)", "1");
shouldBe("layoutTestController.pageNumberForElementById('thirdPage', 1000, 1000)", "2");
shouldBe("layoutTestController.pageNumberForElementById('thirdPage2', 1000, 1000)", "2");

document.body.removeChild(document.getElementById("sandbox"));

var successfullyParsed = true;
