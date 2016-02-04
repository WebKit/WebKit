ControlsTest = class ControlsTest {
    constructor(mediaURL, eventTrigger)
    {
        if (window.testRunner) {
            testRunner.dumpAsText();
            testRunner.waitUntilDone();
        }

        this.callback = null;
        this.mediaURL = mediaURL || "../content/test";
        this.eventTrigger = eventTrigger || "canplaythrough";
        this.currentMessage = "";
        this.currentValue = null;
        this.cachedCurrentState = null;
    }

    whenReady(callback)
    {
        if (typeof callback == "function")
            this.callback = callback;
        return this;
    }

    get currentState()
    {
        if (!this.media)
            return null;

        if (window.internals) {
            this.cachedCurrentState = JSON.parse(internals.getCurrentMediaControlsStatusForElement(this.media));
            return this.cachedCurrentState;
        }

        // This is only for fallback testing. Even then it is pretty useless.
        return { idiom: "apple", status: "fail" };
    }

    get video()
    {
        return this.media;
    }

    stateForControlsElement(elementName, flushCachedState)
    {
        if (flushCachedState)
            this.cachedCurrentState = null;
            
        var state = this.cachedCurrentState || this.currentState;
        if (state.elements && state.elements.length) {
            for (var i = 0; i < state.elements.length; i++) {
                if (state.elements[i].name == elementName)
                    return state.elements[i];
            }
        }
        return null;
    }

    handleEvent(event)
    {
        this.logMessage(`EVENT: ${event.type}`);
        if (event.type == this.eventTrigger && this.callback && window.testRunner)
            this.callback();
    }

    logSuccess(msg)
    {
        this.logMessage(`PASS: ${msg}`);
    }

    logFailure(msg)
    {
        this.logMessage(`FAIL: ${msg}`);
    }

    logMessage(msg)
    {
        this.console.appendChild(document.createTextNode(msg));
        this.logBlankLine();
    }

    logBlankLine()
    {
        this.console.appendChild(document.createElement("br"));
    }

    startNewSection(msg)
    {
        this.logBlankLine();
        this.logMessage(msg);
        this.logBlankLine();
    }

    setup()
    {
        this.console = document.createElement("div");
        this.console.className = "console";
        document.body.appendChild(this.console);

        this.media = document.querySelector("video");

        if (!this.media) {
            this.logFailure("Unable to find media element");
            this.end();
            return false;
        }

        this.media.addEventListener(this.eventTrigger, this, false);
        this.media.src = findMediaFile("video", this.mediaURL);

        if (!window.testRunner) {
            this.logFailure("Test requires DRT.");
            return false;
        }

        return true;
    }

    start(msg)
    {
        window.addEventListener("load", function () {
            if (!this.setup())
                return;

            if (msg)
                this.startNewSection(msg);
        }.bind(this), false);
        return this;
    }

    end()
    {
        this.logMessage("");
        this.logMessage("Testing finished.");
        if (window.testRunner)
            testRunner.notifyDone();
        return this;
    }

    test(message)
    {
        this.currentMessage = message;
        return this;
    }

    value(newValue)
    {
        this.currentValue = newValue;
        return this;
    }

    isEqualTo(value)
    {
        if (this.currentValue == value)
            this.logSuccess(this.currentMessage);
        else
            this.logFailure(`${this.currentMessage} Expected: "${value}". Actual: "${this.currentValue}"`);
    }

    isNotEqualTo(value)
    {
        if (this.currentValue != value)
            this.logSuccess(this.currentMessage);
        else
            this.logFailure(`${this.currentMessage} Expected to not be equal to: "${value}". Actual: "${this.currentValue}"`);
    }

    contains(value)
    {
        if (this.currentValue.indexOf(value) >= 0)
            this.logSuccess(this.currentMessage);
        else
            this.logFailure(`${this.currentMessage} Expected to contain "${value}". Actual: "${this.currentValue}"`);
    }

    doesNotContain(value)
    {
        if (this.currentValue.indexOf(value) < 0)
            this.logSuccess(this.currentMessage);
        else
            this.logFailure(`${this.currentMessage} Expected to not contain "${value}". Actual: "${this.currentValue}"`);
    }

    isEmptyString()
    {
        if (!this.currentValue && typeof this.currentValue == "string")
            this.logSuccess(this.currentMessage);
        else
            this.logFailure(`${this.currentMessage} Expected an empty string. Actual: "${this.currentValue}"`);
    }

    isTrue()
    {
        if (this.currentValue)
            this.logSuccess(this.currentMessage);
        else
            this.logFailure(`${this.currentMessage} Expected a true value. Actual: "${this.currentValue}"`);
    }

    isFalse()
    {
        if (!this.currentValue)
            this.logSuccess(this.currentMessage);
        else
            this.logFailure(`${this.currentMessage} Expected a false value. Actual: "${this.currentValue}"`);
    }

    isNotZero()
    {
        if (this.currentValue)
            this.logSuccess(this.currentMessage);
        else
            this.logFailure(`${this.currentMessage} Expected a non-zero value`);
    }

    isLessThan(value)
    {
        if (this.currentValue < value)
            this.logSuccess(this.currentMessage);
        else
            this.logFailure(`${this.currentMessage} Actual: "${this.currentValue}" is not less than Expected: "${value}"`);
    }

    isGreaterThan(value)
    {
        if (this.currentValue > value)
            this.logSuccess(this.currentMessage);
        else
            this.logFailure(`${this.currentMessage} Actual: "${this.currentValue}" is not greater than Expected: "${value}"`);
    }

}
