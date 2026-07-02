const sourceOrigin = "https://127.0.0.1:8443/";
const crossOrigin = "https://localhost:8443/";
const downgradedOrigin = "http://127.0.0.1:8000/";
const fullSourceURL = "fullSourceURL";

let frames = [];
let framesMultipart = [];
let resultCount = 0;

function printResults(resultsArray, isTestingMultipart)
{
    for (let i = 0; i < resultsArray.length; i++) {
        let currentTest = tests[i];
        actualReferrer = resultsArray[i]
        debug("Testing 'Referrer-Policy: " + referrerPolicy + "' - referrer origin: " + sourceOrigin + " - destination origin: " + currentTest[1] + " - isMultipartResponse? " + isTestingMultipart);
        if (currentTest[0] === fullSourceURL)
            shouldBeEqualToString("actualReferrer", sourceOrigin + "security/resources/serve-referrer-policy-and-test.py?value=" + referrerPolicy + "&destinationOrigin=" + currentTest[1] + "&isTestingMultipart=" + (+isTestingMultipart) + "&id=" + i);
        else
            shouldBeEqualToString("actualReferrer", "" + currentTest[0]);
        debug("");
    }
}

onmessage = (msg) => {
    if (msg.data.isTestingMultipart === "1") {
        resultsMultipart[msg.data.id] = msg.data.referer;
        framesMultipart[msg.data.id]?.remove();
    } else {
        results[msg.data.id] = msg.data.referer;
        frames[msg.data.id]?.remove();
    }

    if (++resultCount == (results.length + resultsMultipart.length)) {
        printResults(results, false);
        printResults(resultsMultipart, true);
        finishJSTest();
    }
        
}

async function runTests(isTestingMultipart)
{
    for (let i = 0; i < tests.length; i++) {
        let currentTest = tests[i];
        let frame = document.createElement("iframe");
        frame.style = "display:none";
        frame.src = sourceOrigin + "security/resources/serve-referrer-policy-and-test.py?value=" + referrerPolicy + "&destinationOrigin=" + currentTest[1] + "&isTestingMultipart=" + (isTestingMultipart ? "1" : "0") + "&id=" + i;
        document.body.appendChild(frame);

        if (isTestingMultipart) {
            framesMultipart[i] = frame;
        } else {
            frames[i] = frame;
        } 

        await new Promise(resolve => frame.onload = resolve);
    }
}
