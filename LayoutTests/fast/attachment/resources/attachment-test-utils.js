function assertInAttachmentTest(successOrExpressionString, expressionString) {
    if (!expressionString) {
        expressionString = successOrExpressionString;
        successOrExpressionString = eval(successOrExpressionString);
    }
    console.assert(successOrExpressionString, expressionString);
    if (!successOrExpressionString) {
        const d = document.body.appendChild(document.createElement("div"));
        d.style.border = "2px solid red";
        const error = "ASSERTION FAILURE: " + expressionString;
        d.innerText = error;
        document.documentElement.classList.remove("reftest-wait");
        throw new Error(error);
    }
}

function takeScreenshot() {
    assertInAttachmentTest('document.documentElement.classList.contains("reftest-wait")');
    document.documentElement.classList.remove("reftest-wait");
}

function takeScreenshotWhenAttachmentsSettled(message) {
    assertInAttachmentTest('document.documentElement.classList.contains("reftest-wait")');

    const attachments = [...document.getElementsByTagName("attachment")];
    assertInAttachmentTest(attachments.length, "attachments.length");
    if (!attachments.length)
        return takeScreenshot();

    const states = new Map(attachments.map(a => [a, []]));

    const onFailure = () => {
        for (const [attachment, events] of states)
            attachment.insertAdjacentText("afterend", "<- (" + message + ") - events = [" + events.join() + "]");
        takeScreenshot();
    };

    const timeoutId = setTimeout(onFailure, 5000);

    const onSuccess = () => {
        clearTimeout(timeoutId);
        takeScreenshot();
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

function AugmentDivWithAttachmentTree() {
    const isIOSFamily = window.testRunner && window.testRunner.isIOSFamily;
    const isVision = isIOSFamily && window.testRunner.isVision;

    let style = "div.attachment { display: inline-block; margin: 1px; } ";
    if (window.internals) {
        style += window.internals.attachmentElementShadowUserAgentStyleSheet();

        style = style.replaceAll("#attachment-", ".attachment-");
        // Hard-coded values because these -apple-xxx colors are only available in UA style sheets.
        style = style.replaceAll("-apple-system-tertiary-fill", isIOSFamily ? (isVision ? "rgb(118 118 128 / 0.24)" : "rgb(118 118 128 / 0.12)") : "rgb(0 0 0 / 0.047)");
        style = style.replaceAll("-apple-system-quaternary-fill", isIOSFamily ? (isVision ? "rgb(118 118 128 / 0.18)" : "rgb(116 116 128 / 0.08)") : "rgb(0 0 0 / 0.03)");
    }

    let styleElement = document.createElement("style");
    styleElement.innerHTML = style;
    document.head.appendChild(styleElement);

    const iconSize = isIOSFamily ? (isVision ? 40 : 72) : 52;

    const attachments = [...document.getElementsByClassName("attachment")];
    for (attachment of attachments) {
        function addChild(parent, type, className, innerText)
        {
            let child = document.createElement(type);
            if (attachment.id) {
                child.id = attachment.id + "-" + className;
            }
            child.className = className;
            child.style = style;
            if (innerText !== undefined) {
                child.innerText = innerText;
            }
            parent.appendChild(child);
            return child;
        }

        let action = "";
        let title = "";
        let subtitle = "";
        let progress = "0";
        for (const attr of attachment.attributes) {
            switch (attr.name) {
            case "action": action = attr.value; break;
            case "title": title = attr.value; break;
            case "subtitle": subtitle = attr.value; break;
            case "progress": progress = attr.value; break;
            }
        }

        // Same tree structure as created in HTMLAttachmentElement::ensureWideLayoutShadowTree().
        let container = addChild(attachment, "div", "attachment-container");
        container.style = `--icon-size: ${iconSize}px`;
        {
            let background = addChild(container, "div", "attachment-background");
            {
                let previewArea = addChild(background, "div", "attachment-preview-area");
                {
                    let image = addChild(previewArea, "img", "attachment-icon");
                    let placeholder = addChild(previewArea, "div", "attachment-placeholder");
                    let progressElement = addChild(previewArea, "div", "attachment-progress");

                    // FIXME: Handle non-progress with fake icon.
                    if (progress == "0") {
                        image.style = "display: none";
                        progressElement.style = "display: none";
                    } else {
                        image.style = "display: none";
                        placeholder.style = "display: none";
                        progressElement.style = "--progress: " + progress;
                    }
                }

                let informationArea = addChild(background, "div", "attachment-information-area");
                {
                    let informationBlock = addChild(informationArea, "div", "attachment-information-block");
                    {
                        let actionText = addChild(informationBlock, "div", "attachment-action", action);
                        let titleText = addChild(informationBlock, "div", "attachment-title", title);
                        let subtitleText = addChild(informationBlock, "div", "attachment-subtitle", subtitle);
                        // FIXME: Implement save button.
                    }
                }
            }
        }
    }
}

function AugmentAttachmentClassElementsWithAttachmentTree()
{
    const attachments = [...document.getElementsByClassName("attachment")];
    for (attachment of attachments) {
        AugmentDivWithAttachmentTree(attachment);
    }
}
