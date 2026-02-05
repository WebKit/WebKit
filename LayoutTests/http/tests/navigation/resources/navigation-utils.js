const sameSiteHostname = "127.0.0.1";
const crossSiteHostname = "localhost";

/* `page` must be defined in embedding HTML files */
function makeMessage(action, arg = "") {
    if (page === null || page === undefined) {
        console.error("makeMessage: page is not defined");
        return "";
    }
    return `${page}:${action}:${arg}:${location.search}`;
}

function parseMessage(message) {
    const [sender, action, arg, search] = message.split(":", 4);
    return {sender, action, arg, search};
}

function sendMessageTo(action, arg, target) {
    const message = makeMessage(action, arg);
    console.log(`${page}: Sending message: ${message}`);
    target.postMessage(message, "*");
}

function dispatchMessage(message, senderHandler) {
    console.log(`${page}: Received message: ${message}`);
    const {sender, action, arg, search} = parseMessage(message);
    const actionHandlers = senderHandler[sender];
    if (actionHandlers) {
        if (typeof actionHandlers === "function") {
            actionHandlers({action, arg, search});
        } else {
            const handler = actionHandlers[action];
            if (handler) {
                handler({arg, search});
            }
        }
    }
}

function forwardMessageToOpener(message) {
    if (!window.opener) {
        console.error("forwardMessageToOpener: window.opener is not defined");
        return;
    }
    window.opener.postMessage(message, "*");
}

/* `popup` must be defined in embedding HTML files */
function sendMessageToPopup(action, arg = "") {
    if (popup === null || popup === undefined) {
        console.error("sendMessageToPopup: popup is not defined");
        return;
    }
    sendMessageTo(action, arg, popup);
}

function sendMessageToOpener(action, arg = "") {
    if (!window.opener) {
        console.error("sendMessageToOpener: window.opener is not defined");
        return;
    }
    sendMessageTo(action, arg, window.opener);
}

function sendMessageToTop(action, arg = "") {
    if (!window.top) {
        console.error("sendMessageToTop: window.top is not defined");
        return;
    }
    sendMessageTo(action, arg, window.top);
}

/* `iframeId` must be defined in embedding HTML files */
function iframeContentWindow() {
    if (iframeId === null || iframeId === undefined) {
        console.error("getIframe: iframeId is not defined");
        return;
    }
    return document.getElementById(iframeId).contentWindow;

}

function sendMessageToIframe(action, arg = "") {
    sendMessageTo(action, arg, iframeContentWindow());
}

function navigateTo(url) {
    window.location.href = url;
}

function navigateIframeTo(url) {
    iframeContentWindow().location.href = url;
}
