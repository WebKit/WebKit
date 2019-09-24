import {EventStream} from './Ref.js';

class AssertFailedError extends Error {
    constructor(msg) {
        super(msg)
    }
}

function stripRef(html) {
    return html.replace(/ref="[\w\d\-]+"/g, "")
}

class Expect {
    constructor(valueA) {
        this.valueA = valueA;
        this.valueB = null;
        return this;
    }

    isType(type) {
        if (type === null) {
            if (this.valueA !== null)
                throw new AssertFailedError(`${this.valueA} should be null`);
        } else if (Number.isNaN(type) || type === "NaN") {
            if (!Number.isNaN(this.valueA))
                throw new AssertFailedError(`${this.valueA} should be NaN`);    
        } else if (type === "Array" || type === "array" || type === Array) {
            if (!Array.isArray(this.valueA))
                throw new AssertFailedError(`${this.valueA} should be an Array`);
        } else if (typeof this.valueA !== type && false === this.valueA instanceof type)
            throw new AssertFailedError(`${this.valueA} should be type ${type}`);
        return this;
    }

    equalToValue(valueB) {
        if (this.valueA !== valueB)
            throw new AssertFailedError(`${this.valueA} should equal to ${valueB}`);
    }

    equalToHtmlWithoutRef(html) {
        return this.equalToValue(stripRef(this.valueA), stripRef(html));
    }

    equalToArray(array, compare = (x, y) => expect(x).equalToValue(y)) {
        expect(this.valueA).isType("Array");
        expect(array).isType("Array");
        expect(this.valueA.length).equalToValue(array.length);
        for (let i = 0; i < this.valueA.length; i++) {
            compare(this.valueA[i], array[i]);
        }
    }

    notEqualToValue(valueB) {
        if (this.valueA === valueB)
            throw new AssertFailedError(`${this.valueA} should not equal to ${valueB}`);
    }

    greaterThan(valueB) {
        if (this.valueA <= valueB)
            throw new AssertFailedError(`${this.valueA} should greater than ${valueB}`);
    }

    greaterThanOrEqualTo(valueB) {
        if (this.valueA < valueB)
            throw new AssertFailedError(`${this.valueA} should greater than or equal to ${valueB}`);
    }

    lessThan(valueB) {
        if (this.valueA >= valueB)
            throw new AssertFailedError(`${this.valueA} should less than ${valueB}`);
    }

    lessThanOrEqualTo(valueB) {
        if (this.valueA > valueB)
            throw new AssertFailedError(`${this.valueA} should less than or equal to ${valueB}`);
    }
}

function expect(value) {
    return new Expect(value);
}

class TestSuite {
    expect(value) {
        return expect(value);
    }

    sleep(milliseconds) {
        return new Promise((resolve) => {
            setTimeout(resolve, milliseconds);
        });
    }

    waitForSignal(singal, name, timeout=1000) {
        return new Promise((resolve, reject) => {
            const handler = () => {
                clearTimeout(timeoutHandler);
                resolve();
                singal.removeListener(handler);
            };
            const timeoutHandler = setTimeout(() => {
                singal.removeListener(handler);
                reject(new AssertFailedError(`Cannot get the ${name} signal after ${timeout} ms`));
            }, timeout);
            singal.addListener(handler);
        });
    }

    waitForRefMounted(ref, timeout=1000) {
        return this.waitForSignal(ref.onElementMount, "mount", timeout);
    }

    waitForRefUnmounted(ref, timeout=1000) {
        return this.waitForSignal(ref.onElementUnmount, "unmount", timeout);
    }

    waitForStateUpdated(ref, timeout=1000) {
        return this.waitForSignal(ref.onStateUpdate, "state update", timeout);
    }

    async setup() {}
    async clearUp() {}
}


function getTestFucntionNames(testObj) {
    const fixedMethods = new Set(Object.getOwnPropertyNames(TestSuite.prototype));
    const testObjMethods = Object.getOwnPropertyNames(testObj.constructor.prototype);
    const testMethods = [];
    for (let method of testObjMethods) {
        if (!fixedMethods.has(method))
            testMethods.push(method);
    }
    return testMethods;
}

const TEST_RESULT_TYPE = Object.freeze({
    Success: Symbol("Success"),
    Error: Symbol("Error"),
    Failed: Symbol("Failed"),
});

class TestResult {
    constructor(className, fnName) {
        this.className = className;
        this.fnName = fnName;
        this.exception = null;
        this.type = TEST_RESULT_TYPE.Success;
    }
    
    catchException(e) {
        this.exception = e;
        console.error(e);
        if (e instanceof AssertFailedError)
            this.type = TEST_RESULT_TYPE.Failed;
        else
            this.type = TEST_RESULT_TYPE.Error;
    }
}

async function getTestResult(obj, fnName, args = []) {
    const result = new TestResult(obj.constructor.name, fnName);
    try {
        await obj[fnName](...args);
    } catch (e) {
        result.catchException(e);
    }
    return result;
}

class TestController {
    constructor(setupArgs) {
        this.allTests = {}
        this.resultsEs = new EventStream();
        this.setupArgs = Array.isArray(setupArgs) ? setupArgs : [];
    }
    
    addResultHandler(handler) {
        this.resultsEs.action(handler);
    }
    
    addSetupArgs(args) {
        this.setupArgs = this.setupArgs.concat(args);
    }

    collect(testSuiteClass) {
        const testInstance = new testSuiteClass();
        const testName = testInstance.constructor.name;
        if (testName in this.allTests) {
            throw new Error(`${testName} has already been collected`);
        }
        this.allTests[testName] = testInstance;
    }
    
    async collectFile(filePath) {
        const testModule = await import(filePath);
        Object.keys(testModule).forEach(className => this.collect(testModule[className]));
    }

    async runTest(testName, fnName) {
        let haveError = false;
        const testInstance = this.allTests[testName];
        const testMethods = getTestFucntionNames(testInstance);
        let result = await getTestResult(testInstance, "setup", this.setupArgs);
        this.resultsEs.add(result);
        if (result.type === TEST_RESULT_TYPE.Success) {
            for (let testMethodName of testMethods) {
                if (fnName && fnName !== testMethodName) 
                    return;
                result = await getTestResult(testInstance, testMethodName);
                this.resultsEs.add(result);
                if (result.type !== TEST_RESULT_TYPE.Success)
                    haveError = true;
            }
        } else 
            haveError = true;
        result = await getTestResult(testInstance, "clearUp");
        this.resultsEs.add(result);
        if (result.type !== TEST_RESULT_TYPE.Success)
            haveError = true;
        return haveError;
    }

    async run(testSuiteClassName, testFnName) {
        let haveError = false;
        if (testSuiteClassName)
            haveError = await this.runTest(testSuiteClassName, testFnName);
        else {
            for(let testSuiteClassName of Object.keys(this.allTests))
                haveError |= await this.runTest(testSuiteClassName);
        }
        const finalResult = new TestResult("", "");
        if (!haveError)
            finalResult.type = TEST_RESULT_TYPE.Success;
        else
            finalResult.type = TEST_RESULT_TYPE.Failed;
        this.resultsEs.add(finalResult);
    }
}

export {TestSuite, TestController, TEST_RESULT_TYPE}
