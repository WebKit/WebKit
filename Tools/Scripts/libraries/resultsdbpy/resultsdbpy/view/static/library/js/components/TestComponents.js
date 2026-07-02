import {DOM, REF} from "../Ref.js"
import {TestController, TEST_RESULT_TYPE} from "../Test.js"

function TestApp(testController) {
    return (
        `<div class="row content">
            <div class="col-6">
                ${TestResultConsole(testController)}
            </div>
            <div class="col-6">
                ${TestRenderArea(testController)}
            </div>
        </div>`
    );
}

function TestResult(testResult) {
    let statusClass = "success";
    let description = testResult.className ? "PASS" : "ALL PASS";
    switch (testResult.type) {
        case TEST_RESULT_TYPE.Failed:
            statusClass = "failed";
            description = "failed"
        break;
        case TEST_RESULT_TYPE.Error:
            statusClass = "error";
            description = "raise an error";
        break;
    }
    return (
        `<div class="text">
         ${testResult.className ? "" : "<hr>"}
            <div class="text block">
                <a href="?className=${testResult.className}">${testResult.className}</a>
            </div>
            <div class="text block">
                <a href="?className=${testResult.className}&fnName=${testResult.fnName}">${testResult.fnName}</a>
            </div>
            <div class="text block ${statusClass}">
                ${description}
            </div>
            ${testResult.exception ? `
                <div class="text ${statusClass}">
                    <pre>${testResult.exception.message}\n${testResult.exception.stack}</pre>
                </div>` : ""}
        </div>`
    );
}

function TestResultConsole(testController) {
    const ref = REF.createRef({
        onStateUpdate: (element, stateDiff) => {
            if (stateDiff.addTestResult) {
                DOM.append(element, TestResult(stateDiff.addTestResult));
                if (!stateDiff.addTestResult.className) {
                    let final_res = "ALL PASS";
                    if (stateDiff.addTestResult.type !== TEST_RESULT_TYPE.Success)
                        final_res = "FAILED";
                    document.title = `${final_res}`;
                }
            }
        }
    });
    testController.addResultHandler(testResult => ref.setState({addTestResult: testResult}));
    return `<div ref="${ref}"></div>`;
}


function TestRenderArea(testController) {
    const ref = REF.createRef({
        onElementMount: (element) => {
            testController.addSetupArgs([element]);
            const url = new URL(window.location);
            const searchParam = new URLSearchParams(url.search);
            const className = searchParam.get("className");
            if (className)
                document.title = className;
            const fnName = searchParam.get("fnName");
            testController.run(className, fnName)
        } 
    });
    return `<div ref="${ref}"></div>`;
}

export {TestApp};
