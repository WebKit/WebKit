function switchCenter(input)
{
    const rect = input.getBoundingClientRect();
    const frameRect = input.ownerDocument.defaultView.frameElement?.getBoundingClientRect();
    return {
        x: Math.round(rect.left + rect.width / 2 + (frameRect?.left ?? 0) + scrollX),
        y: Math.round(rect.top + rect.height / 2 + (frameRect?.top ?? 0) + scrollY),
    };
}

// A touch while a scroller is still moving stops it rather than reaching the switch.
async function settle(scroller = document.scrollingElement)
{
    let stable = 0;
    let previous = -1;
    for (let i = 0; i < 100 && stable < 4; ++i) {
        stable = previous === Math.round(scroller.scrollTop) ? stable + 1 : 0;
        previous = Math.round(scroller.scrollTop);
        await UIHelper.delayFor(50);
    }
    await UIHelper.ensurePresentationUpdate();
}

// Each step is { wait: seconds } or { move: [x, y] }, with the move relative to where the touch started.
async function touch(input, steps, { scroller, waitOutDoubleTapInterval = true } = { })
{
    const { x, y } = switchCenter(input);
    const stream = new UIHelper.EventStreamBuilder().begin(x, y);
    for (const step of steps) {
        if (step.wait)
            stream.wait(step.wait);
        if (step.move)
            stream.move(x + step.move[0], y + step.move[1], 0.25);
    }
    await UIHelper.sendEventStream(stream.end().takeResult());
    await settle(scroller);
    if (waitOutDoubleTapInterval)
        await UIHelper.delayFor(500);
}
