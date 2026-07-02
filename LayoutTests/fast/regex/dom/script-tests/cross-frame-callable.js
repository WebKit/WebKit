function doTest(childRegExp)
{
    re = childRegExp;
    shouldThrow("re('a')");
    shouldBe("re.exec('a')", "['a']");
}

var iframe = document.createElement('iframe');
document.body.appendChild(iframe);
iframe.contentDocument.write('<script>top.doTest(/a/)</script>');
iframe.contentDocument.close();
document.write('DONE');
