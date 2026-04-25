const VERTICAL = [0, 1];
const HORIZONTAL = [1, 0];
const VERTICAL_AND_HORIZONTAL = [1, 1];

async function doScrollGlide(targetElement, direction)
{
    // These arrays are arguments that will be passed to eventSender.mouseScrollByWithWheelAndMomentumPhases.
    // Depending on whether or not the direction is vertical or horizontal one of the first two elements
    // of each entry will be zeroed out, so that eventSender only scrolls in one direction at a time.
    let scrollMotions = [
        [-1, -1, 'began', 'none'],
        [-1, -1, 'changed', 'none'],
        [-1, -1, 'changed', 'none'],
        [-1, -1, 'changed', 'none'],
        [0, 0, 'ended', 'none'],
        [-1, -1, 'none', 'begin'],
        [-1, -1, 'none', 'continue'],
        [0, 0, 'none', 'end'],
    ];

    return doScrollTest(targetElement, direction, scrollMotions);
}

async function doScrollSnap(targetElement, direction)
{
    // These arrays are arguments that will be passed to eventSender.mouseScrollByWithWheelAndMomentumPhases.
    // Depending on whether or not the direction is vertical or horizontal one of the first two elements
    // of each entry will be zeroed out, so that eventSender only scrolls in one direction at a time.
    let scrollMotions = [
        [-1, 0, 'began', 'none'],
        [-1, 0, 'changed', 'none'],
        [-1, 0, 'changed', 'none'],
        [0, 0, 'ended', 'none'],
    ];
    return doScrollTest(targetElement, direction, scrollMotions);
}

async function doScrollTest(targetElement, direction, scrollMotions)
{
    var startPosX;
    var startPosY;
    if (document.scrollingElement == targetElement) {
        // getBoundingClientRect() can be an out-of-viewport rectangle if the
        // root scrolling element is the targetElement.
        startPosX = 0;
        startPosY = 0;
    } else {
        var clientRect = targetElement.getBoundingClientRect();
        var startPosX = clientRect.x;
        var startPosY = clientRect.y;
    }

    // Slightly offset the mouse position so that the events are inside the element.
    startPosX += 5;
    startPosY += 5;

    await UIHelper.startMonitoringWheelEvents();
    eventSender.mouseMoveTo(startPosX, startPosY);

    // `direction` is a two-element array with a one in the appropriate direction.
    scrollMotions.forEach((callArguments) => {
        callArguments[0] *= direction[0];
        callArguments[1] *= direction[1];
        eventSender.mouseScrollByWithWheelAndMomentumPhases(...callArguments);
    });

    await UIHelper.waitForScrollCompletion();
}
