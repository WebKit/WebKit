const sourceOrigin = "https://127.0.0.1:8443/";
const crossOrigin = "https://localhost:8443/";
const downgradedOrigin = "http://127.0.0.1:8000/";
const fullSourceURL = "fullSourceURL";

// [Referrer-Policy header, expected referrer, destination origin].
let tests = [
    ["no-referrer", "", crossOrigin],
    ["no-referrer", "", sourceOrigin],
    ["no-referrer", "", downgradedOrigin],
    ["origin", sourceOrigin, crossOrigin],
    ["origin", sourceOrigin, sourceOrigin],
    ["origin", sourceOrigin, downgradedOrigin],
    ["unsafe-url", fullSourceURL, crossOrigin],
    ["unsafe-url", fullSourceURL, sourceOrigin],
    ["unsafe-url", fullSourceURL, downgradedOrigin],
    ["no-referrer-when-downgrade", fullSourceURL, crossOrigin],
    ["no-referrer-when-downgrade", fullSourceURL, sourceOrigin],
    ["no-referrer-when-downgrade", "", downgradedOrigin],
    ["same-origin", "", crossOrigin],
    ["same-origin", fullSourceURL, sourceOrigin],
    ["same-origin", "", downgradedOrigin],
    ["strict-origin", sourceOrigin, crossOrigin],
    ["strict-origin", sourceOrigin, sourceOrigin],
    ["strict-origin", "", downgradedOrigin],
    ["strict-origin-when-cross-origin", sourceOrigin, crossOrigin],
    ["strict-origin-when-cross-origin", fullSourceURL, sourceOrigin],
    ["strict-origin-when-cross-origin", "", downgradedOrigin],
    ["origin-when-cross-origin", sourceOrigin, crossOrigin],
    ["origin-when-cross-origin", fullSourceURL, sourceOrigin],
    ["origin-when-cross-origin", sourceOrigin, downgradedOrigin],
    ["invalid", sourceOrigin, crossOrigin],
    ["invalid", fullSourceURL, sourceOrigin],
    ["invalid", "", downgradedOrigin],
    ["", sourceOrigin, crossOrigin],
    ["", fullSourceURL, sourceOrigin],
    ["", "", downgradedOrigin],
];

let results = new Array(tests.length);
let resultCount = 0;

function printResults()
{
    for (let i = 0; i < results.length; i++) {
        let currentTest = tests[i];
        actualReferrer = results[i]
        debug("Testing 'Referrer-Policy: " + currentTest[0] + "' - referrer origin: " + sourceOrigin + " - destination origin: " + currentTest[2] + " - isMultipartResponse? " + isTestingMultipart);
        if (currentTest[1] === fullSourceURL)
            shouldBeEqualToString("actualReferrer", sourceOrigin + "security/resources/serve-referrer-policy-and-test.py?value=" + currentTest[0] + "&destinationOrigin=" + currentTest[2] + "&isTestingMultipart=" + (isTestingMultipart ? "1" : "0") + "&id=" + i);
        else
            shouldBeEqualToString("actualReferrer", "" + currentTest[1]);
        debug("");
    }
    finishJSTest();
}

onmessage = (msg) => {
    results[msg.data.id] = msg.data.referer;
    if (++resultCount == results.length)
        printResults();
}

async function runTests(isTestingMultipart)
{
    window.isTestingMultipart = isTestingMultipart;
    for (let i = 0; i < results.length; i++) {
        let currentTest = tests[i % tests.length];
        let isTestingMultipart = i >= tests.length;

        let frame = document.createElement("iframe");
        frame.style = "display:none";
        frame.src = sourceOrigin + "security/resources/serve-referrer-policy-and-test.py?value=" + currentTest[0] + "&destinationOrigin=" + currentTest[2] + "&isTestingMultipart=" + (isTestingMultipart ? "1" : "0") + "&id=" + i;
        document.body.appendChild(frame);

        await new Promise(resolve => frame.onload = resolve);
    }
}
