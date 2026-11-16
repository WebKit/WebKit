var paddingMsg = " (no author specified padding)";
var inputs = document.querySelectorAll("input");
var textInput = inputs[0];
var testInput = inputs[1];
var baseWidth = textInput.offsetWidth;

function assertLogicalWidthsEqual(isVertical) {
    if (isVertical)
        shouldBe('testInput.offsetHeight', 'baseWidth');
    else
        shouldBe('testInput.offsetWidth', 'baseWidth');
    debug("\n");
}
function setWritingMode(writingMode) {
    inputs.forEach((input) => {
        input.style.writingMode = writingMode;
    });
}

function testVerticalLR() {
    setWritingMode("vertical-lr");
    baseWidth = textInput.offsetHeight;
    assertLogicalWidthsEqual(true);
    setWritingMode("");
}

function testVerticalRL() {
    setWritingMode("vertical-rl");
    baseWidth = textInput.offsetHeight;
    assertLogicalWidthsEqual(true);
    setWritingMode("");
}

function testSidewaysLR() {
    setWritingMode("sideways-lr");
    baseWidth = textInput.offsetHeight;
    assertLogicalWidthsEqual(true);
    setWritingMode("");
}

function testSidewaysRL() {
    setWritingMode("sideways-rl");
    baseWidth = textInput.offsetHeight;
    assertLogicalWidthsEqual(true);
    setWritingMode("");
}

function test() {
    debug("Testing left-to-right." + paddingMsg);
    assertLogicalWidthsEqual(false);

    debug("Testing vertical-rl." + paddingMsg);
    testVerticalRL();

    debug("Testing vertical-lr." + paddingMsg);
    testVerticalLR();

    debug("Testing sideways-lr." + paddingMsg);
    testSidewaysLR();

    debug("Testing sideways-rl." + paddingMsg);
    testSidewaysLR();

    debug("Testing right-to-left." + paddingMsg);
    inputs.forEach((input) => {
        input.setAttribute("dir","RTL");
    });
    baseWidth = textInput.offsetWidth;
    assertLogicalWidthsEqual(false);

    debug("Testing vertical-rl with dir=rtl." + paddingMsg);
    testVerticalRL();

    debug("Testing vertical-lr with dir=rtl." + paddingMsg);
    testVerticalLR();

    debug("Testing sideways-lr with dir=rtl." + paddingMsg);
    testSidewaysLR();

    debug("Testing sideways-rl with dir=rtl." + paddingMsg);
    testSidewaysLR();
}
test();

paddingMsg = "(padding-inline-start: 10px)"
inputs.forEach((input) => {
    input.style.paddingInlineStart = "10px";
});
baseWidth = textInput.offsetWidth;
test();

paddingMsg = "(padding-inline-end: 10px)"
inputs.forEach((input) => {
    input.style.paddingInlineEnd = "10px";
});
baseWidth = textInput.offsetWidth;
test();

// type=search is not tested intentionally.

document.getElementById('parent').innerHTML = '';
