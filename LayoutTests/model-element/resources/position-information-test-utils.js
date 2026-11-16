async function scroll() {
    var fromX = overflow_container.offsetLeft + 50;
    var fromY = overflow_container.offsetTop + overflow_container.offsetHeight - 5;
    var toX = fromX;
    var toY = fromY - 100;
    await touchAndDragFromPointToPoint(fromX, fromY, toX, toY);
    await liftUpAtPoint(toX, toY);
    overflow_container.scrollTop = 0;
}

async function scrollAndCheckIfPositionInformationUpdated(correctValue) {
    await scroll();
    didWait = await UIHelper.didCallEnsurePositionInformationIsUpToDateSinceLastCheck();
    const msg = "didCallEnsurePositionInformationIsUpToDateSinceLastCheck was " + didWait + ", expected " + correctValue;
    if (didWait == correctValue)
        testPassed(msg);
    else
        testFailed(msg);
}

function sendMessageToFrame(iframe, action, count = 0) {
    return new Promise((resolve) => {
        const messageHandler = (event) => {
            if (event.data.action === action && event.data.completed) {
                window.removeEventListener('message', messageHandler);
                resolve(event.data);
            }
        };
        window.addEventListener('message', messageHandler);
        iframe.contentWindow.postMessage({ action, count }, '*');
    });
}

async function addModelsToFrame(iframe, count) {
    return await sendMessageToFrame(iframe, 'add', count);
}

async function removeModelsFromFrame(iframe, count) {
    return await sendMessageToFrame(iframe, 'remove', count);
}

async function clearModelsInFrame(iframe) {
    return await sendMessageToFrame(iframe, 'clear', 0);
}

async function navigateFrameToEmpty(iframe) {
    return new Promise((resolve) => {
        const messageHandler = (event) => {
            if (event.data.action === 'frameLoaded' && event.data.completed) {
                window.removeEventListener('message', messageHandler);
                resolve();
            }
        };
        window.addEventListener('message', messageHandler);
        iframe.contentWindow.postMessage({ action: 'navigate' }, '*');
    });
}
