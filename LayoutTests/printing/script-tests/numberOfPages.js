description("Test for layoutTestController.numberOfPages()");

function createParagraph(id)
{
    var element = document.createElement("div");
    element.id = id;
    element.width = 100;
    document.getElementById("sandbox").appendChild(element);
    return element;
}

var firstPage = createParagraph("firstPage");
firstPage.style.height = 700;

var secondPage = createParagraph("secondPage");
secondPage.style.height = 700;

shouldBe("layoutTestController.numberOfPages(1000, 1000)", "2");
shouldBe("layoutTestController.numberOfPages(10000, 10000)", "1");

document.body.removeChild(document.getElementById("sandbox"));

var successfullyParsed = true;
