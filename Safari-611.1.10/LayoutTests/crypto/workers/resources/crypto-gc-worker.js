importScripts('../../../resources/js-test-pre.js');

description("Test that self.crypto wrapper preserves custom properties.");
jsTestIsAsync = true;

function startTest()
{
    self.crypto.foo = "bar";
    gc();
    setTimeout(continueTest, 10);
}

function continueTest()
{
    gc();
    setTimeout(finishTest, 10);
}

function finishTest()
{
    gc();
    shouldBe('self.crypto.foo', '"bar"');
    finishJSTest();
}

startTest();
