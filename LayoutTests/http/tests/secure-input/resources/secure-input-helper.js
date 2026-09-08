function waitForIframeLoad(iframe) {
    return new Promise(resolve => iframe.addEventListener("load", resolve, { once: true }));
}

function waitForMessage(expectedData) {
    return new Promise(resolve => {
        window.addEventListener("message", function handler(event) {
            if (event.data !== expectedData)
                return;
            window.removeEventListener("message", handler);
            resolve();
        });
    });
}

async function focusCrossOriginIframePassword(iframe) {
    let focusedPromise = waitForMessage("password-focused");
    await UIHelper.activateElement(iframe);
    await focusedPromise;
}
