// Helpers to reach into the PDF.js viewer that WebKit builds for a PDF document.
// The viewer lives in a webkit-pdfjs-viewer:// iframe inside the PDF document, which is
// only reachable from a test because layout tests allow universal access from file URLs.

function loadPDFInFrame(url, width = 700, height = 400)
{
    const frame = document.createElement("iframe");
    frame.style.width = `${width}px`;
    frame.style.height = `${height}px`;
    frame.src = url;
    document.body.appendChild(frame);
    return frame;
}

function viewerForFrame(frame)
{
    return frame.contentDocument?.querySelector("iframe")?.contentWindow?.PDFViewerApplication;
}

// Waits for the metadata rather than just the document, because reading the metadata is what
// makes PDF.js log the line about the document it opened: waiting for it keeps that log line
// in the same place in every test's output.
async function waitForPDFToLoad(frame)
{
    while (!viewerForFrame(frame)?.documentInfo)
        await new Promise(resolve => setTimeout(resolve, 0));
    return viewerForFrame(frame);
}
