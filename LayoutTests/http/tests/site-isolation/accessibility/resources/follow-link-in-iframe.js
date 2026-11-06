// Loads the iframe with id "iframe" with a page holding a link to targetURL, presses that link
// through the accessibility API (as VoiceOver would), and checks that the accessibility tree
// switches from the iframe's old page to its new one.

function headingCount(element, title) {
    let count = element.role.includes("AXHeading") && element.title == title ? 1 : 0;
    for (let i = 0; i < element.childrenCount; i++)
        count += headingCount(element.childAtIndex(i), title);
    return count;
}

function findLink(element) {
    if (element.role.includes("AXLink") && element.title == "Follow link")
        return element;
    for (let i = 0; i < element.childrenCount; i++) {
        const link = findLink(element.childAtIndex(i));
        if (link)
            return link;
    }
    return null;
}

function dumpTree(element, indent) {
    let result = "";
    const role = element.role;
    const matched = ["AXWebArea", "AXHeading", "AXLink", "AXButton"].some(r => role.endsWith(r));
    if (matched)
        result += `${"    ".repeat(indent)}${role} ${element.title}\n`;
    for (let i = 0; i < element.childrenCount; i++)
        result += dumpTree(element.childAtIndex(i), matched ? indent + 1 : indent);
    return result;
}

async function waitForHeading(title) {
    // Cross-process frames can take longer to be exposed than waitFor() allows.
    for (let attempt = 0; attempt < 100; attempt++) {
        if (headingCount(accessibilityController.rootElement, title))
            return true;
        await sleep(100);
    }
    return false;
}

async function followLinkInIframe(initialURL, targetURL) {
    let output = "";
    accessibilityController.setClientAccessibilityMode(true);

    const iframe = document.getElementById("iframe");
    await new Promise(resolve => {
        iframe.addEventListener("load", resolve, { once: true });
        iframe.src = `${initialURL}?target=${encodeURIComponent(targetURL)}`;
    });

    if (!await waitForHeading("Heading before navigation"))
        output += "FAIL: the iframe's initial page never appeared in the accessibility tree.\n";
    output += `Before following the link:\n${dumpTree(accessibilityController.rootElement, 0)}\n`;

    const link = findLink(accessibilityController.rootElement);
    if (!link) {
        output += "FAIL: could not find the link in the iframe.\n";
        return output;
    }

    await new Promise(resolve => {
        iframe.addEventListener("load", resolve, { once: true });
        link.press();
    });

    if (!await waitForHeading("Heading after navigation"))
        output += "FAIL: the iframe's new page never appeared in the accessibility tree.\n";
    output += `After following the link:\n${dumpTree(accessibilityController.rootElement, 0)}`;
    return output;
}
