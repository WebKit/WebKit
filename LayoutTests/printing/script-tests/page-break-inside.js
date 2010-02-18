description("Test for page-break-inside:avoid");

function createParagraph(id, height)
{
    var element = document.createElement("div");
    element.id = id;
    element.style.height = height;
    element.appendChild(document.createTextNode("foobar"));
    document.getElementById("sandbox").appendChild(element);
    return element;
}

var pageHeightInPixels = 1000;

function pageNumber(id) {
  return layoutTestController.pageNumberForElementById(id, 1000, pageHeightInPixels);
}

createParagraph("page1", 900).style.pageBreakBefore = "always";
createParagraph("page2", 900).style.pageBreakInside = "avoid";

createParagraph("page3-1", 900).style.pageBreakBefore = "always";
createParagraph("page3-2", 100).style.pageBreakInside = "avoid";

createParagraph("page4", 900).style.pageBreakBefore = "always";
createParagraph("page5", 101).style.pageBreakInside = "avoid";

createParagraph("page6", 100).style.pageBreakBefore = "always";
createParagraph("page7", 2100).style.pageBreakInside = "avoid";
createParagraph("page9", 100);

createParagraph("page10-1", 100).style.pageBreakBefore = "always";
createParagraph("page10-2", 100).style.pageBreakAfter = "always";
// Make sure page-break only happens once, not twice.
createParagraph("page11", 900).style.pageBreakInside = "avoid";

// Before calling "shouldBe" tests, sets "display: none" style to |console| element so that the results of pageNumber should not be affected.
// FIXME: Use functions defined in printing/resources/paged-media-test-utils.js instead of this hack.
document.getElementById("console").style.display = 'none';

shouldBe("pageNumber('page1')", "1");
shouldBe("pageNumber('page2')", "2");

shouldBe("pageNumber('page3-1')", "3");
shouldBe("pageNumber('page3-2')", "3");

shouldBe("pageNumber('page4')", "4");
shouldBe("pageNumber('page5')", "5");

shouldBe("pageNumber('page6')", "6");
shouldBe("pageNumber('page7')", "7");
shouldBe("pageNumber('page9')", "9");

shouldBe("pageNumber('page10-1')", "10");
shouldBe("pageNumber('page10-2')", "10");
shouldBe("pageNumber('page11')", "11");

document.getElementById("console").style.display = 'block';

document.body.removeChild(document.getElementById("sandbox"));

var successfullyParsed = true;
