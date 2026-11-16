// Shared by the two display-mode cross-origin tests (applicationmanifest/ and site-isolation/).
const sameOriginURL =
    "http://127.0.0.1:8000/applicationmanifest/resources/display-mode-application-iframe.html";
const crossOriginURL =
    "http://localhost:8000/applicationmanifest/resources/display-mode-application-iframe.html";
const nestedEmbedderURL =
    "http://localhost:8000/applicationmanifest/resources/display-mode-application-nested-embedder.html";

function log(message) {
    const paragraph = document.createElement("p");
    paragraph.textContent = message;
    document.body.appendChild(paragraph);
}

function loadIframe(source, sandbox) {
    return new Promise((resolve) => {
        const iframe = document.createElement("iframe");
        if (sandbox) iframe.sandbox = sandbox;
        iframe.src = source;
        iframe.onload = () => resolve(iframe);
        document.body.appendChild(iframe);
    });
}

function getReport(iframe) {
    return new Promise((resolve) => {
        const handler = (event) => {
            if (event.source !== iframe.contentWindow) return;
            window.removeEventListener("message", handler);
            resolve(event.data);
        };
        window.addEventListener("message", handler);
        iframe.contentWindow.postMessage("report", "*");
    });
}

async function logIframeReport(label, source, sandbox) {
    const iframe = await loadIframe(source, sandbox);
    const report = await getReport(iframe);
    log(
        `${label} (origin ${report.origin}): application=${report.application} notApplication=${report.notApplication} standalone=${report.standalone}`,
    );
}
