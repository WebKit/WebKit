// Global object for use in js-test assertions.
var scrollElement;

// Tweak UIHelper.callFunctionAndWaitForTargetScrollToFinish() to rely on "scrollend" when possible.
const callFunctionAndWaitForTargetScrollToFinish =
      UIHelper.isWebKit2() ?
      async (scrollTarget, functionToCall) => UIHelper.callFunctionAndWaitForEvent(functionToCall, scrollTarget, "scrollend") :
      UIHelper.callFunctionAndWaitForTargetScrollToFinish;

// See WPT's dom/events/scrolling/scroll_support.js
async function waitForScrollReset(scrollTarget, scrollElement, x = 0, y = 0) {
    if (scrollElement.scrollTop == x && scrollElement.scrollLeft == y)
        return;
    await callFunctionAndWaitForTargetScrollToFinish(scrollTarget, () => scrollElement.scrollTo(x, y));
}

// Run scroll-axis-lock test. The config parameter accepts these keys:
// - scroller/iframe/doc: Element, or iframe or document whose scrolling
//   is tested.
// - scroll_is_axis_locked: A boolean indicating whether the previous
//   object is expected to perform axis-locked scroll on the tested
//   platform.
async function runTest(config) {
    const scrollTarget =
          config.scroller || config.iframe?.contentDocument || config.doc;
    scrollElement = config.scroller ||
        config.iframe?.contentDocument.scrollingElement ||
        config.doc?.scrollingElement;
    async function performAlmostVerticalScrollDown() {
      if (UIHelper.isIOSFamily()) {
        await callFunctionAndWaitForTargetScrollToFinish(scrollTarget, async () => {
          await UIHelper.dragFromPointToPoint(150, 300, 149, 50, 0.1);
        });
      } else {
        // On Desktop, perform "almost vertical scroll" using mouse wheel.
        // mouseScrollByWithWheelAndMomentumPhases does not seem to
        // handle nonzero deltaX and deltaY values at the same time and it
        // accumulates deltas of consecutive calls, so perform calls for
        // vertical and horizontal components, with an animation frame
        // in between.
        // Additionally, this test actually follows the implementation of
        // ScrollingEffectsController::handleWheelEvent: it starts with
        // the large vertical scroll first (causing it to be stored into
        // m_cumulativeGestureDelta) and the small horizontal scroll that
        // comes after is thus vertically axis-locked.
        await UIHelper.startMonitoringWheelEvents();
        eventSender.mouseMoveTo(50, 50);
        eventSender.mouseScrollByWithWheelAndMomentumPhases(0, -20, 'began', 'none');
        await UIHelper.animationFrame();
        eventSender.mouseScrollByWithWheelAndMomentumPhases(-1, 0, 'changed', 'none');
        eventSender.mouseScrollByWithWheelAndMomentumPhases(0, 0, "ended", "none");
        await UIHelper.waitForScrollCompletion();
      }
    }

    // Initial scroll offset.
    await waitForScrollReset(scrollTarget, scrollElement, 0, 0);
    shouldBe('scrollElement.scrollTop', '0');
    shouldBe('scrollElement.scrollLeft', '0');

    // scroll-axis-lock: none, verify an "almost vertical scroll down" affects x offset.
    scrollElement.style.scrollAxisLock = 'none';
    shouldBeEqualToString('getComputedStyle(scrollElement).scrollAxisLock', 'none');
    await UIHelper.renderingComplete();
    await performAlmostVerticalScrollDown();
    shouldBeGreaterThan('scrollElement.scrollTop', '0');
    shouldBeGreaterThan('scrollElement.scrollLeft', '0');

    // Reset scroll offset.
    await waitForScrollReset(scrollTarget, scrollElement, 0, 0);
    shouldBe('scrollElement.scrollTop', '0');
    shouldBe('scrollElement.scrollLeft', '0');

    // scroll-axis-lock: auto, verify an "almost vertical scroll down" may be x-axis-locked.
    scrollElement.style.scrollAxisLock = 'auto';
    shouldBeEqualToString('getComputedStyle(scrollElement).scrollAxisLock', 'auto');
    await UIHelper.renderingComplete();
    await performAlmostVerticalScrollDown();
    shouldBeGreaterThan('scrollElement.scrollTop', '0');
    if (config.scroll_is_axis_locked)
        shouldBe('scrollElement.scrollLeft', '0');
    else
        shouldBeGreaterThan('scrollElement.scrollLeft', '0');

    finishJSTest();
}
