// Helpers for tests that load one iframe at a time and query frames that reply to posted messages.

let iframe;

function loadIframe(src)
{
    return new Promise((resolve) => {
        addEventListener('message', function listener(event) {
            if (!event.data.loaded)
                return;
            removeEventListener('message', listener);
            resolve();
        });
        iframe?.remove();
        iframe = document.createElement('iframe');
        iframe.src = src;
        document.body.appendChild(iframe);
    });
}

function sendToFrame(target, message, key)
{
    return new Promise((resolve) => {
        addEventListener('message', function listener(event) {
            if (event.data[key] === undefined)
                return;
            removeEventListener('message', listener);
            resolve(event.data);
        });
        target.postMessage(message, '*');
    });
}
