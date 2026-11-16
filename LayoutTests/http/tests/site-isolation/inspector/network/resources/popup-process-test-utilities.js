let popup;
let popupProcessID;
let started = false;

function startTest() {
    if (started)
        return;
    started = true;
    runTest();
}

function addFrame(url) {
    let iframe = document.createElement("iframe");
    iframe.id = iframe.name = "popup-process-frame";
    iframe.src = url;
    document.body.appendChild(iframe);
}

function closePopup() {
    popup.close();
}

// The popup has an opener, so it joins this page's browsing context group. It must exist before the inspector attaches.
function openPopup() {
    window.addEventListener("message", (event) => {
        if (event.data.type === "processId") {
            popupProcessID = event.data.pid;
            startTest();
        } else if (event.data.type === "iframeProcessId")
            TestPage.dispatchEventToFrontend("IFrameProcessID", event.data.pid);
    });
    popup = window.open("http://localhost:8000/site-isolation/resources/report-process-id.html");
    setTimeout(startTest, 5000);
}
