var scroller;

// Tweak UIHelper.callFunctionAndWaitForTargetScrollToFinish() to rely on "scrollend" when possible.
const callFunctionAndWaitForTargetScrollToFinish =
      UIHelper.isWebKit2() ?
      (scrollTarget, functionToCall) => UIHelper.callFunctionAndWaitForEvent(functionToCall, scrollTarget, "scrollend") :
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
    scroller = config.scroller ||
        config.iframe?.contentDocument.scrollingElement ||
        config.doc?.scrollingElement
    const bounds =
          (config.scroller || config.iframe)?.getBoundingClientRect() || {
              left: 0,
              top: 0,
              width: config.doc?.defaultView.innerWidth,
              height: config.doc?.defaultView.innerHeight,
          };

    async function performAlmostVerticalScrollDown() {
        await callFunctionAndWaitForTargetScrollToFinish(scrollTarget, async () => {
            const centerX = bounds.left + bounds.width / 2;
            const centerY = bounds.top + bounds.height / 2;
            if (UIHelper.isIOSFamily()) {
                await UIHelper.dragFromPointToPoint(centerX, centerY, centerX - 1, centerY - 200, 0.15);
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
                eventSender.mouseMoveTo(centerX, centerY);
                eventSender.mouseScrollByWithWheelAndMomentumPhases(0, -20, 'began', 'none');
                await UIHelper.animationFrame();
                eventSender.mouseScrollByWithWheelAndMomentumPhases(-1, 0, 'changed', 'none');
                eventSender.mouseScrollByWithWheelAndMomentumPhases(0, 0, "ended", "none");
                await UIHelper.waitForTargetScrollAnimationToSettle(scrollTarget);
            }
        });
    }

    // Initial scroll offset.
    await waitForScrollReset(scrollTarget, scroller, 0, 0);
    shouldBe('scroller.scrollTop', '0');
    shouldBe('scroller.scrollLeft', '0');

    // scroll-axis-lock: none, verify an "almost vertical scroll down" affects x offset.
    scroller.style.scrollAxisLock = 'none';
    shouldBeEqualToString('getComputedStyle(scroller).scrollAxisLock', 'none');
    await UIHelper.renderingComplete();
    await performAlmostVerticalScrollDown();
    shouldBeGreaterThan('scroller.scrollTop', '0');
    shouldBeGreaterThan('scroller.scrollLeft', '0');

    // Reset scroll offset.
    await waitForScrollReset(scrollTarget, scroller, 0, 0);
    shouldBe('scroller.scrollTop', '0');
    shouldBe('scroller.scrollLeft', '0');

    // scroll-axis-lock: auto, verify an "almost vertical scroll down" may be x-axis-locked.
    scroller.style.scrollAxisLock = 'auto';
    shouldBeEqualToString('getComputedStyle(scroller).scrollAxisLock', 'auto');
    await UIHelper.renderingComplete();
    await performAlmostVerticalScrollDown();
    shouldBeGreaterThan('scroller.scrollTop', '0');
    if (config.scroll_is_axis_locked)
        shouldBe('scroller.scrollLeft', '0');
    else
        shouldBeGreaterThan('scroller.scrollLeft', '0');

    finishJSTest();
}
