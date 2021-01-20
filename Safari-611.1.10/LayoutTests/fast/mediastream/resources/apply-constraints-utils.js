let settings;
let error;

ConstraintsTest = class ConstraintsTest {

    constructor(constraints, tests, description)
    {
        this.constraints = constraints;
        this.tests = tests;
        this.description = description;

        window.jsTestIsAsync = true;
        window.successfullyParsed = true;
        if (window.testRunner)
            testRunner.setUserMediaPermission(true);
    }

    onStreamReady(callback)
    {
        if (typeof callback == "function")
            this.streamCallback = callback;
        return this;
    }

    onVideoReady(callback)
    {
        if (typeof callback == "function")
            this.videoCallback = callback;
        return this;
    }

    scheduleNextTest()
    {
        new Promise(resolved => this.runNextTest()); 
    }

    checkTrackSettings()
    {
        settings = this.track.getSettings();
        for (let property in this.currentTest.expected) {
            let expected = this.currentTest.expected[property];
            if (typeof expected === "string")
                shouldBeEqualToString(`settings['${property}']`, expected);
            else
                shouldEvaluateTo(`settings['${property}']`, expected);
        }
    }

    runNextTest()
    {
        description(this.description);

        debug("");
        if (!this.tests.length) {
            finishJSTest();
            return;
        }

        this.currentTest = this.tests.shift();
        debug(`** Constraint: ${JSON.stringify(this.currentTest.constraint)} - ${this.currentTest.message}`);
        this.track.applyConstraints(this.currentTest.constraint)
            .then(() => {
                if (this.currentTest.error)
                    testFailed(`Constraint '${this.currentTest.error}' should have failed to apply, is '${settings[this.currentTest.error]}'`);
                else
                    this.checkTrackSettings()
                this.scheduleNextTest();
            })
            .catch((evt) => {
                if (!this.currentTest.error) {
                    testFailed("Promise was rejected");
                    testFailed(`Constraint failed to apply: ${evt} - constraint = ${evt.constraint}`);
                } else {
                    testPassed("Promise was rejected");
                    error = evt;
                    shouldBeEqualToString("error.constraint", this.currentTest.error);
                }
                this.checkTrackSettings()
                this.scheduleNextTest();
            });
    }
    
    setStreamTrack(track)
    {
        this.track = track;
    }

    start()
    {
        window.addEventListener("load", function () {

            navigator.mediaDevices.getUserMedia(this.constraints)
                .then(stream => {
                    this.video = document.querySelector("video");
                    this.video.srcObject = stream;
                    if (this.streamCallback)
                        this.streamCallback(stream);
                })
                .then(() => new Promise(resolve => this.video.onloadedmetadata = resolve))
                .then(() => {
                    if (this.videoCallback)
                        this.videoCallback(this.video);
                    this.runNextTest();
                })
                .catch(err => {
                    testFailed(`Stream setup failed with error: ${err}`);
                    finishJSTest();
                });

        }.bind(this), false);

        return this;
    }

}
