// Helpers for talking to a subframe that follows the now-playing-frame.html message protocol: it replies with
// { type, value } (or { type: "error", message }) to queries posted to its window.

function waitForSubframeMessage(replyType) {
    return new Promise((resolve, reject) => {
        const timeoutId = setTimeout(() => {
            window.removeEventListener("message", handler);
            reject(new Error(`timed out waiting for "${replyType}" from the subframe`));
        }, 10000);
        function handler(event) {
            if (!event.data || (event.data.type !== replyType && event.data.type !== "error"))
                return;
            window.removeEventListener("message", handler);
            clearTimeout(timeoutId);
            if (event.data.type === "error")
                reject(new Error(event.data.message));
            else
                resolve(event.data.value);
        }
        window.addEventListener("message", handler);
    });
}

function askFrame(frame, query, replyType) {
    const reply = waitForSubframeMessage(replyType);
    frame.contentWindow.postMessage(query, "*");
    return reply;
}

function subframeIsActiveNowPlaying(frame) {
    return askFrame(frame, "isActiveNowPlaying?", "isActiveNowPlaying");
}

function subframeIsPaused(frame) {
    return askFrame(frame, "isPaused?", "isPaused");
}
