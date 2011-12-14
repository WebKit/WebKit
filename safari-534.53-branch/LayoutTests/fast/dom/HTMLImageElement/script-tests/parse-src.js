description('Test that when using the JavaScript bindings to get the value of a URL attribute such \
    as \'src\', leading whitespace is ignored');

function testURL(attribute, url)
{
    img[attribute] = url;
    return img[attribute];
}

var img = new Image();
var url = "about:blank";

shouldBe('testURL("src", url)', 'url');
shouldBe('testURL("src", "\\n" + url)', 'url');
shouldBe('testURL("src", " " + url)', 'url');

shouldBe('testURL("lowsrc", url)', 'url');
shouldBe('testURL("lowsrc", "\\n" + url)', 'url');
shouldBe('testURL("lowsrc", " " + url)', 'url');

shouldBe('testURL("longDesc", url)', 'url');
shouldBe('testURL("longDesc", "\\n" + url)', 'url');
shouldBe('testURL("longDesc", " " + url)', 'url');

var successfullyParsed = true;
