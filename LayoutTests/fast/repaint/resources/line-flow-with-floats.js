var loadCount = 0;

async function loaded()
{
    window.testRunner?.waitUntilDone();
    if (++loadCount < 2)
        return;
    document.body.offsetTop;
    test(iframe.contentDocument);
    window.testRunner?.notifyDone();
}
