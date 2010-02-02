description("Test for page-break-before:always and page-break-after:always");

function createParagraph(id)
{
    var element = document.createElement("p");
    element.id = id;
    element.appendChild(document.createTextNode("foobar"));
    document.getElementById("sandbox").appendChild(element);
    return element;
}

createParagraph("firstPage");
createParagraph("secondPage1").style.pageBreakBefore = "always";
createParagraph("secondPage2").style.pageBreakAfter = "always";
createParagraph("thirdPage").style.pageBreakBefore = "always";

shouldBe("layoutTestController.pageNumberForElementById('firstPage')", "0");
shouldBe("layoutTestController.pageNumberForElementById('secondPage1')", "1");
shouldBe("layoutTestController.pageNumberForElementById('secondPage2')", "1");
shouldBe("layoutTestController.pageNumberForElementById('thirdPage')", "2");

document.body.removeChild(document.getElementById("sandbox"));

var successfullyParsed = true;
