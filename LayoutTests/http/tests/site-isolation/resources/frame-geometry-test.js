// Loaded by the main frame of the frame geometry tests. We pass every iframe in the test pages a
// "label" query parameter, which is used to track the frame geometry of each of the iframes.

// Need to poll since frame geometry arrives at cross-site frames asynchronously.
const POLL_INTERVAL_MS = 25;
const POLL_TIMEOUT_MS = 4000;

// The last frame geometry report for each frame.
const reports = { };

window.addEventListener("message", function (event) {
    if (event.data && event.data.label)
        reports[event.data.label] = event.data;
}, false);

function requestReports()
{
    for (let i = 0; i < window.length; i++)
        window[i].postMessage("report-frame-geometry", "*");
}

function geometryRect(report, field)
{
    if (!report || !report[field])
        return null;

    const [x, y, width, height] = report[field];
    return {
        x, y, width, height,
        right: x + width,
        bottom: y + height,
        isEmpty: width <= 0 && height <= 0,
        toString() { return `${x},${y} ${width}x${height}`; }
    };
}

// Polls until the predicate returns true or a timeout occurs.
async function waitForRect(label, field, predicate)
{
    const deadline = performance.now() + POLL_TIMEOUT_MS;

    while (true) {
        requestReports();
        await new Promise(resolve => setTimeout(resolve, POLL_INTERVAL_MS));

        const rect = geometryRect(reports[label], field);
        if (rect && predicate(rect))
            return rect;

        if (performance.now() > deadline)
            return rect;
    }
}

async function assertRect(label, field, expected, description)
{
    const rect = await waitForRect(label, field, rect => rect.toString() === expected);
    assert_equals(rect ? rect.toString() : `no report from "${label}"`, expected, description);
}

const waitForClipRect = (label, predicate) => waitForRect(label, "windowClipRect", predicate);
const waitForExposedRect = (label, predicate) => waitForRect(label, "exposedContentRect", predicate);
const assertClipRect = (label, expected, description) => assertRect(label, "windowClipRect", expected, description);
const assertExposedRect = (label, expected, description) => assertRect(label, "exposedContentRect", expected, description);

// Waits for each frame to report frame geometry once.
async function waitForFirstReports(labels)
{
    const deadline = performance.now() + POLL_TIMEOUT_MS;

    while (labels.some(label => !reports[label])) {
        requestReports();
        await new Promise(resolve => setTimeout(resolve, POLL_INTERVAL_MS));

        if (performance.now() > deadline)
            return false;
    }
    return true;
}
