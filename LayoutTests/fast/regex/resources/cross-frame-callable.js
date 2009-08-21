function doTest(childRegExp)
{
    re = childRegExp;
    shouldBe("re('a')", "['a']");
}

var iframe = document.createElement('iframe');
document.body.appendChild(iframe);
iframe.contentDocument.write('<script>top.doTest(/a/)</script>');
document.write('DONE');

var successfullyParsed = true;
