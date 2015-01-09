// To verify event / touch coordinates.

var expectedPageX, expectedPageY, expectedClientX, expectedClientY;
function setExpectedValues(pageX, pageY, clientX, clientY) {
    expectedPageX = pageX;
    expectedPageY = pageY;
    expectedClientX = clientX;
    expectedClientY = clientY;
}

var eventOrTouch;
function verifyCoordinates(o) {
    eventOrTouch = o;
    shouldBe('eventOrTouch.pageX', expectedPageX.toString());
    shouldBe('eventOrTouch.pageY', expectedPageY.toString());
    shouldBe('eventOrTouch.clientX', expectedClientX.toString());
    shouldBe('eventOrTouch.clientY', expectedClientY.toString());
}

// To verify client rect positions.

var expectedLeft, expectedRight, expectedTop, expectedBottom, expectedWidth, expectedHeight;
function setExpectedClientRectValues(left, right, top, bottom, width, height) {
    expectedLeft = left;
    expectedRight = right;
    expectedTop = top;
    expectedBottom = bottom;
    expectedWidth = width;
    expectedHeight = width;
}

var clientRect;
function verifyClientRect(o) {
    clientRect = o;
    shouldBe('clientRect.left', expectedLeft.toString());
    shouldBe('clientRect.right', expectedRight.toString());
    shouldBe('clientRect.top', expectedTop.toString());
    shouldBe('clientRect.bottom', expectedBottom.toString());
    shouldBe('clientRect.width', expectedWidth.toString());
    shouldBe('clientRect.height', expectedHeight.toString());
}

// To verify page scroll offsets.

var expectedScrollX, expectedScrollY;
function setExpectedScrollOffsets(x, y) {
    expectedScrollX = x;
    expectedScrollY = y;
}

function verifyScrollOffsets() {
    shouldBe('window.scrollX', expectedScrollX.toString());
    shouldBe('window.pageXOffset', expectedScrollX.toString());
    shouldBe('document.body.scrollLeft', expectedScrollX.toString());
    shouldBe('window.scrollY', expectedScrollY.toString());
    shouldBe('window.pageYOffset', expectedScrollY.toString());
    shouldBe('document.body.scrollTop', expectedScrollY.toString());
}

// To verify visible window sizes.

var expectedWindowWidth, expectedWindowHeight
function setExpectedWindowSize(width, height) {
    expectedWindowWidth = width;
    expectedWindowHeight = height;
}

// From Element.cpp:
//   - When in strict mode, clientWidth/Height for the document element should return the width/height of the containing frame.
//   - When in quirks mode, clientWidth/Height for the body element should return the width/height of the containing frame.

function verifyWindowSize() {
    shouldBe('window.innerWidth', expectedWindowWidth.toString());
    shouldBe('window.innerHeight', expectedWindowHeight.toString());
    shouldBe('document.documentElement.clientWidth', expectedWindowWidth.toString());
    shouldBe('document.documentElement.clientHeight', expectedWindowHeight.toString());
}

function verifyWindowSizeQuirks() {
    shouldBe('window.innerWidth', expectedWindowWidth.toString());
    shouldBe('window.innerHeight', expectedWindowHeight.toString());
    shouldBe('document.body.clientWidth', expectedWindowWidth.toString());
    shouldBe('document.body.clientHeight', expectedWindowHeight.toString());
}

// Asynchronous tests. (Viewport scaling, Touch events, etc).

if (window.testRunner)
    testRunner.waitUntilDone();

function endTest() {
    if (window.testRunner) {
        successfullyParsed = true;
        isSuccessfullyParsed();
        testRunner.notifyDone();
    }
}

// Zoom and pan the page.

function setInitialScaleAndPanBy(zoomScale, panX, panY) {
    // Scale with: <meta name="viewport" content="initial-scale=2">.
    var meta = document.createElement('meta');
    meta.setAttribute('name', 'viewport');
    meta.setAttribute('content', 'initial-scale=' + zoomScale);
    document.head.appendChild(meta);

    // Scrolling is Panning with a "porthole".
    window.scrollBy(panX, panY);
}
