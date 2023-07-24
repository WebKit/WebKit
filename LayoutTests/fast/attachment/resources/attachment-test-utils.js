function takeScreenshotWhenAttachmentsSettled() {
    console.assert(document.documentElement.classList.contains("reftest-wait"));

    const attachments = [...document.getElementsByTagName("attachment")];
    console.assert(attachments.length);
    if (!attachments.length)
        return document.documentElement.classList.remove("reftest-wait");

    const states = new Map(attachments.map(a => [a, []]));

    const onFailure = () => {
        for (const [attachment, events] of states)
            attachment.insertAdjacentText("afterend", "<- (" + message + ") - events = [" + events.join() + "]");
        document.documentElement.classList.remove("reftest-wait");
    };

    const timeoutId = setTimeout(onFailure, 5000);

    const onSuccess = () => {
        clearTimeout(timeoutId);
        document.documentElement.classList.remove("reftest-wait");
    };

    const promises = attachments.map((attachment) => new Promise((resolve, reject) => {
        for (const event of ["beforeload", "loadingerror", "loadeddata", "error", "load"])
            attachment.addEventListener(event, () => { states.get(attachment).push(event); });
        attachment.addEventListener("loadingerror", reject, { once: true });
        attachment.addEventListener("load", () => {
            requestAnimationFrame(() => requestAnimationFrame(resolve));
        }, { once: true });
    }));

    Promise.all(promises).then(onSuccess, onFailure);
}

function takeActualScreenshotWhenAttachmentsSettled() {
    takeScreenshotWhenAttachmentsSettled("actual");
}

function takeExpectedScreenshotWhenAttachmentsSettled() {
    takeScreenshotWhenAttachmentsSettled("expected");
}
