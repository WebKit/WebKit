function recordWheel(event)
{
    debug('wheel event ' + wheelEventsOccurred + '+> ' + event.target);
    wheelEventsOccurred++;
}

function recordScroll(event)
{
    debug('scroll event ' + scrollEventsOccurred + '+> ' + event.target);
    scrollEventsOccurred++;

    if (window.eventSender) {
        // Because scroll events arrive asynchronously, only one will arrive.
        if (gesturesOccurred == expectedGesturesTotal) {
            shouldBe('scrollEventsOccurred', expectedScrollEventsOccurred);
            // If we've got here, we've passed.
            successfullyParsed = true;
            isSuccessfullyParsed();
            if (window.testRunner)
                testRunner.notifyDone();
        }
    }
}

function exitIfNecessary()
{
    debug('gesture events not implemented on this platform or gesture event scrolling of a document is broken');
    successfullyParsed = true;
    isSuccessfullyParsed();
    if (window.testRunner)
        testRunner.notifyDone();
}

function checkTestDependencies()
{
    return eventSender.gestureScrollBegin && eventSender.gestureScrollUpdate && eventSender.gestureScrollEnd;
}

function checkScrollOffset()
{
    if (window.eventSender) {
        shouldBe(scrolledElement + '.' + 'scrollTop', scrollAmountY[gesturesOccurred]);
        shouldBe(scrolledElement + '.' + 'scrollLeft', scrollAmountX[gesturesOccurred]);
        shouldBe('wheelEventsOccurred', expectedWheelEventsOccurred[gesturesOccurred]);
        gesturesOccurred++;
    }
    if (gesturesOccurred < expectedGesturesTotal) {
        wheelEventsOccurred = 0;
        secondGestureScroll();
    }
}
