function receiveMessage(event) {
    if (event.origin === "https://localhost:8443") {
        if (event.data.indexOf("PASS ") !== -1)
            testPassed(event.data.replace("PASS ", ""));
        else
            testFailed(event.data);
    } else
        testFailed("Received a message from an unexpected origin: " + event.origin);
    finishJSTest();
}

function asciiToUint8Array(str)
{
    const chars = [];
    for (var i = 0; i < str.length; ++i)
        chars.push(str.charCodeAt(i));
    return new Uint8Array(chars);
}
