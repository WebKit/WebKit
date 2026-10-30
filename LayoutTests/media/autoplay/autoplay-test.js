async function getAutoplayPolicyForURL(iframe, url) {
    document.body.appendChild(iframe);
    iframe.src = url;
    await waitFor(iframe, 'load', true);

    iframe.contentWindow.postMessage('getAutoplayPolicy', '*');
    let responseEvent = await waitFor(window, 'message', true);

    return responseEvent.data;
}


async function getAutoplayPolicyForURLWithGesture(iframe, url) {
    document.body.appendChild(iframe);
    iframe.src = url;
    await waitFor(iframe, 'load', true);

    internals.withUserGesture(() => {
        iframe.contentWindow.postMessage('getAutoplayPolicy', '*');
    });
    let responseEvent = await waitFor(window, 'message', true);

    return responseEvent.data;
}
