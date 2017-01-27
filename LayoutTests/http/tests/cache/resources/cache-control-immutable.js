function log(text, expected, result)
{
    logdiv.innerText += text + ": " + (result ? "YES" : "NO") + " (expected " + (expected ? "YES" : "NO") + ")\n";
}

function insertIframe(maxAge, loaded) {
    const iframe = document.createElement('iframe');
    document.body.appendChild(iframe);
    iframe.src = "resources/iframe-with-script.cgi?script-cache-control=immutable,max-age=" + maxAge;
    iframe.onload = () => loaded(iframe);
}

function test(maxAge, callback) {
    insertIframe(maxAge, (iframe) => {
        const firstNumber = iframe.contentWindow.randomNumber;
        iframe.onload = () => callback(firstNumber != iframe.contentWindow.randomNumber);
        iframe.contentWindow.location.reload();
    });
}
