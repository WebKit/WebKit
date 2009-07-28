description("This tests createMarkup via insertOrderedList.  It makes sure that we don't delete font styles and text decorations.  This test must be rewritten when insertOrderedList is modified not to use moveParagraph.");

var boundedContainer = document.createElement("div");
var container = document.createElement("div");
boundedContainer.contentEditalbe = false;
boundedContainer.appendChild(container);
container.contentEditable = true;
document.body.appendChild(boundedContainer);

function testCreateMarkup( text )
{
    container.innerHTML = text;

    var s = document.getSelection();
    var r = document.createRange();
    r.setStart(container.firstChild, 0);
    r.setEnd(container.lastChild, 1);
    s.removeAllRanges();
    s.addRange(r);
    document.execCommand("insertOrderedList");
    document.execCommand("insertOrderedList");

    return container.innerHTML;
}

function shouldBeSameAfterCreateMarkup(test)
{
    shouldBe("testCreateMarkup('"+test+"')", "'"+test+"'");
}

shouldBeSameAfterCreateMarkup('<b>hello</b>');
shouldBeSameAfterCreateMarkup('<strong>hello</strong>');
shouldBeSameAfterCreateMarkup('<i>hello</i>');
shouldBeSameAfterCreateMarkup('<em>hello</em>');
shouldBeSameAfterCreateMarkup('<s>hello</s>');
shouldBeSameAfterCreateMarkup('<strike>hello</strike>');
shouldBeSameAfterCreateMarkup('<em><s><u>hello</u></s></em>');
shouldBeSameAfterCreateMarkup('<u><s><em><u>hello</u></em></s></u>');
shouldBeSameAfterCreateMarkup('<i><span style=\"text-decoration: underline overline line-through;\">world</span></i>');
shouldBeSameAfterCreateMarkup('<em><u style="font-weight: bold;">hello</u></em>');
shouldBeSameAfterCreateMarkup('hello <u>world</u> webkit');

document.body.removeChild(boundedContainer);

var successfullyParsed = true;
