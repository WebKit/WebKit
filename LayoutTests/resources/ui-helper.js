
window.UIHelper = class UIHelper {
    static isIOSFamily()
    {
        return testRunner.isIOSFamily;
    }

    static isMac()
    {
        return testRunner.isMac;
    }

    static isWebKit2()
    {
        return testRunner.isWebKit2;
    }

    static doubleClickAt(x, y)
    {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
        eventSender.mouseDown();
        eventSender.mouseUp();
    }

    static doubleClickAtMouseDown(x1, y1)
    {
        eventSender.mouseMoveTo(x1, y1);
        eventSender.mouseDown();
        eventSender.mouseUp();
        eventSender.mouseDown();
    }

    static mouseUp()
    {
        eventSender.mouseUp();
    }

    static doubleClickAtThenDragTo(x1, y1, x2, y2)
    {
        eventSender.mouseMoveTo(x1, y1);
        eventSender.mouseDown();
        eventSender.mouseUp();
        eventSender.mouseDown();
        eventSender.mouseMoveTo(x2, y2);
        eventSender.mouseUp();
    }

    static dragMouseAcrossElement(element)
    {
        const x1 = element.offsetLeft + element.offsetWidth;
        const x2 = element.offsetLeft + element.offsetWidth * .75;
        const x3 = element.offsetLeft + element.offsetWidth / 2;
        const x4 = element.offsetLeft + element.offsetWidth / 4;
        const x5 = element.offsetLeft;
        const y = element.offsetTop + element.offsetHeight / 2;
        eventSender.mouseMoveTo(x1, y);
        eventSender.mouseMoveTo(x2, y);
        eventSender.mouseMoveTo(x3, y);
        eventSender.mouseMoveTo(x4, y);
        eventSender.mouseMoveTo(x5, y);
    }

    static doubleClickElementMouseDown(element1)
    {
        const x1 = element1.offsetLeft + element1.offsetWidth / 2;
        const y1 = element1.offsetTop + element1.offsetHeight / 2;
        return UIHelper.doubleClickAtMouseDown(x1, y1);
    }

    static async moveMouseAndWaitForFrame(x, y)
    {
        eventSender.mouseMoveTo(x, y);
        await UIHelper.animationFrame();
    }

    static async startMonitoringWheelEvents(...args)
    {
        eventSender.monitorWheelEvents(args);
        await UIHelper.ensurePresentationUpdate();
    }

    static async mouseWheelScrollAt(x, y, beginX, beginY, deltaX, deltaY)
    {
        if (beginX === undefined)
            beginX = 0;
        if (beginY === undefined)
            beginY = -1;

        if (deltaX === undefined)
            deltaX = 0;
        if (deltaY === undefined)
            deltaY = -10;

        await UIHelper.startMonitoringWheelEvents();
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseScrollByWithWheelAndMomentumPhases(beginX, beginY, "began", "none");
        eventSender.mouseScrollByWithWheelAndMomentumPhases(deltaX, deltaY, "changed", "none");
        eventSender.mouseScrollByWithWheelAndMomentumPhases(0, 0, "ended", "none");
        await UIHelper.waitForScrollCompletion();
    }

    static async statelessMouseWheelScrollAt(x, y, deltaX, deltaY)
    {
        await UIHelper.startMonitoringWheelEvents();
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseScrollBy(deltaX, deltaY);
        return UIHelper.waitForScrollCompletion();
    }

    static async mouseWheelMayBeginAt(x, y)
    {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseScrollByWithWheelAndMomentumPhases(x, y, "maybegin", "none");
        await UIHelper.animationFrame();
    }

    static async mouseWheelCancelAt(x, y)
    {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseScrollByWithWheelAndMomentumPhases(x, y, "cancelled", "none");
        await UIHelper.animationFrame();
    }

    static async mouseWheelSequence(eventStream, { waitForCompletion = true } = {})
    {
        if (waitForCompletion)
            await UIHelper.startMonitoringWheelEvents();

        const eventStreamAsString = JSON.stringify(eventStream);
        await new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.sendEventStream(\`${eventStreamAsString}\`, () => {
                        uiController.uiScriptComplete();
                    });
                })();
            `, resolve);
        });

        if (waitForCompletion)
            await UIHelper.waitForScrollCompletion();
    }

    static async waitForScrollCompletion()
    {
        if (this.isIOSFamily()) {
            await new Promise(resolve => {
                testRunner.runUIScript(`
                    (function() {
                        uiController.didEndScrollingCallback = function() {
                            uiController.uiScriptComplete();
                        }
                    })()`, resolve);
            });
            // Wait for the new scroll position to get back to the web process.
            return UIHelper.renderingUpdate();
        }

        return new Promise(resolve => {
            eventSender.callAfterScrollingCompletes(() => {
                requestAnimationFrame(resolve);
            });
        });
    }

    static async scrollDown()
    {
        const midX = innerWidth / 2;
        const midY = innerHeight / 2;
        if (!this.isIOSFamily())
            return await this.mouseWheelScrollAt(midX, midY, 0, -1, 0, -100);

        await this.sendEventStream(new this.EventStreamBuilder()
            .begin(midX, midY + 180)
            .move(midX, midY - 180, 0.3)
            .end()
            .takeResult());
    }

    static async animationFrame()
    {
        return new Promise(requestAnimationFrame);
    }

    static async renderingUpdate()
    {
        await UIHelper.animationFrame();
        await UIHelper.delayFor(0);
    }

    static async waitForCondition(conditionFunc)
    {
        while (!conditionFunc()) {
            await UIHelper.animationFrame();
        }
    }

    static async waitForConditionAsync(conditionFunc)
    {
        var condition = await conditionFunc();
        while (!condition) {
            await UIHelper.animationFrame();
            condition = await conditionFunc();
        }
    }

    static sendEventStream(eventStream)
    {
        const eventStreamAsString = JSON.stringify(eventStream);
        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.sendEventStream(\`${eventStreamAsString}\`, () => {
                        uiController.uiScriptComplete();
                    });
                })();
            `, resolve);
        });
    }

    static roundToDevicePixel(value)
    {
        return Math.round(value * devicePixelRatio) / devicePixelRatio;
    }

    static roundRectToDevicePixel(rect)
    {
        return Object.fromEntries(Object.keys(rect).map(key => {
            return [key, this.roundToDevicePixel(rect[key])];
        }));
    }

    static tapAt(x, y, modifiers=[])
    {
        console.assert(this.isIOSFamily());

        if (!this.isWebKit2()) {
            console.assert(!modifiers || !modifiers.length);
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.singleTapAtPointWithModifiers(${x}, ${y}, ${JSON.stringify(modifiers)}, function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static tapElement(element, delay = 0)
    {
        const x = element.offsetLeft + (element.offsetWidth / 2);
        const y = element.offsetTop + (element.offsetHeight / 2);
        this.tapAt(x, y);
    }

    static doubleTapElement(element, delay = 0)
    {
        const x = element.offsetLeft + (element.offsetWidth / 2);
        const y = element.offsetTop + (element.offsetHeight / 2);
        this.doubleTapAt(x, y, delay);
    }

    static doubleTapAt(x, y, delay = 0)
    {
        console.assert(this.isIOSFamily());

        if (!this.isWebKit2()) {
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.doubleTapAtPoint(${x}, ${y}, ${delay}, function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static humanSpeedDoubleTapAt(x, y)
    {
        console.assert(this.isIOSFamily());

        if (!this.isWebKit2()) {
            // FIXME: Add a sleep in here.
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            return Promise.resolve();
        }

        return UIHelper.doubleTapAt(x, y, 0.12);
    }

    static humanSpeedZoomByDoubleTappingAt(x, y)
    {
        console.assert(this.isIOSFamily());

        if (!this.isWebKit2()) {
            // FIXME: Add a sleep in here.
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            return Promise.resolve();
        }

        return new Promise(async (resolve) => {
            testRunner.runUIScript(`
                uiController.didEndZoomingCallback = () => {
                    uiController.didEndZoomingCallback = null;
                    uiController.uiScriptComplete(uiController.zoomScale);
                };
                uiController.doubleTapAtPoint(${x}, ${y}, 0.12, () => { });`, resolve);
        });
    }

    static selectionHitTestOffset()
    {
        return this.isIOSFamily() ? 40 : 0;
    }

    static zoomByDoubleTappingAt(x, y)
    {
        console.assert(this.isIOSFamily());

        if (!this.isWebKit2()) {
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.didEndZoomingCallback = () => {
                    uiController.didEndZoomingCallback = null;
                    uiController.uiScriptComplete(uiController.zoomScale);
                };
                uiController.doubleTapAtPoint(${x}, ${y}, 0, () => { });`, resolve);
        });
    }

    static activateAt(x, y, modifiers=[])
    {
        if (!this.isWebKit2() || !this.isIOSFamily()) {
            eventSender.mouseMoveTo(x, y);
            eventSender.mouseDown(0, modifiers);
            eventSender.mouseUp(0, modifiers);
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.singleTapAtPointWithModifiers(${x}, ${y}, ${JSON.stringify(modifiers)}, function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static activateElement(element, modifiers=[])
    {
        const x = element.offsetLeft + element.offsetWidth / 2;
        const y = element.offsetTop + element.offsetHeight / 2;
        return UIHelper.activateAt(x, y, modifiers);
    }

    static async doubleActivateAt(x, y)
    {
        if (this.isIOSFamily())
            await UIHelper.doubleTapAt(x, y);
        else
            await UIHelper.doubleClickAt(x, y);
    }

    static async doubleActivateAtSelectionStart()
    {
        const rects = window.getSelection().getRangeAt(0).getClientRects();
        const x = rects[0].left;
        const y = rects[0].top;
        if (this.isIOSFamily()) {
            await UIHelper.activateAndWaitForInputSessionAt(x, y);
            await UIHelper.doubleTapAt(x, y);
            // This is only here to deal with async/sync copy/paste calls, so
            // once <rdar://problem/16207002> is resolved, should be able to remove for faster tests.
            await new Promise(resolve => testRunner.runUIScript("uiController.uiScriptComplete()", resolve));
        } else
            await UIHelper.doubleClickAt(x, y);
    }

    static async selectWordByDoubleTapOrClick(element, relativeX = 5, relativeY = 5)
    {
        const boundingRect = element.getBoundingClientRect();
        const x = boundingRect.x + relativeX;
        const y = boundingRect.y + relativeY;
        if (this.isIOSFamily()) {
            await UIHelper.activateAndWaitForInputSessionAt(x, y);
            await UIHelper.doubleTapAt(x, y);
            // This is only here to deal with async/sync copy/paste calls, so
            // once <rdar://problem/16207002> is resolved, should be able to remove for faster tests.
            await new Promise(resolve => testRunner.runUIScript("uiController.uiScriptComplete()", resolve));
        } else {
            await UIHelper.doubleClickAt(x, y);
        }
    }

    static rawKeyDown(key, modifiers=[])
    {
        if (!this.isWebKit2() || !this.isIOSFamily()) {
            eventSender.rawKeyDown(key, modifiers);
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`uiController.rawKeyDown("${key}", ${JSON.stringify(modifiers)});`, resolve);
        });
    }

    static rawKeyUp(key, modifiers=[])
    {
        if (!this.isWebKit2() || !this.isIOSFamily()) {
            eventSender.rawKeyUp(key, modifiers);
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`uiController.rawKeyUp("${key}", ${JSON.stringify(modifiers)});`, resolve);
        });
    }

    static keyDown(key, modifiers=[])
    {
        if (!this.isWebKit2() || !this.isIOSFamily()) {
            eventSender.keyDown(key, modifiers);
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`uiController.keyDown("${key}", ${JSON.stringify(modifiers)});`, resolve);
        });
    }

    static async keyboardScroll(key)
    {
        eventSender.keyDown(key);
        await UIHelper.ensurePresentationUpdate();
    }

    static toggleCapsLock()
    {
        return new Promise((resolve) => {
            testRunner.runUIScript(`uiController.toggleCapsLock(() => uiController.uiScriptComplete());`, resolve);
        });
    }

    static keyboardWillHideCount()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`uiController.keyboardWillHideCount`, result => resolve(parseInt(result)));
        });
    }

    static keyboardIsAutomaticallyShifted()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`uiController.keyboardIsAutomaticallyShifted`, result => resolve(result === "true"));
        });
    }

    static isAnimatingDragCancel()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`uiController.isAnimatingDragCancel`, result => resolve(result === "true"));
        });
    }

    static ensurePresentationUpdate()
    {
        if (!this.isWebKit2())
            return UIHelper.renderingUpdate();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.doAfterPresentationUpdate(function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static ensureStablePresentationUpdate()
    {
        if (!this.isWebKit2())
            return UIHelper.renderingUpdate();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static ensurePositionInformationUpdateForElement(element)
    {
        const boundingRect = element.getBoundingClientRect();
        const x = boundingRect.x + 5;
        const y = boundingRect.y + 5;

        if (!this.isWebKit2()) {
            testRunner.display();
            return Promise.resolve();
        }

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.ensurePositionInformationIsUpToDateAt(${x}, ${y}, function () {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static delayFor(ms)
    {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    static async initiateUserScroll()
    {
        if (this.isIOSFamily()) {
            await UIHelper.scrollTo(0, 10);
        }
        else {
            await UIHelper.statelessMouseWheelScrollAt(10, 10, 0, 10);
        }
    }

    static scrollTo(x, y, unconstrained)
    {
        if (!this.isWebKit2()) {
            window.scrollTo(x, y);
            return Promise.resolve();
        }

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.didEndScrollingCallback = function() {
                        uiController.uiScriptComplete();
                    }
                    uiController.scrollToOffset(${x}, ${y}, { unconstrained: ${unconstrained} });
                })()`, resolve);
        });
    }
    
    static immediateScrollTo(x, y, unconstrained)
    {
        if (!this.isWebKit2()) {
            window.scrollTo(x, y);
            return Promise.resolve();
        }

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.immediateScrollToOffset(${x}, ${y}, { unconstrained: ${unconstrained} });`, resolve);
        });
    }

    static immediateUnstableScrollTo(x, y, unconstrained)
    {
        if (!this.isWebKit2()) {
            window.scrollTo(x, y);
            return Promise.resolve();
        }

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.stableStateOverride = false;
                uiController.immediateScrollToOffset(${x}, ${y}, { unconstrained: ${unconstrained} });`, resolve);
        });
    }

    static immediateScrollElementAtContentPointToOffset(x, y, scrollX, scrollY, scrollUpdatesDisabled = false)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.scrollUpdatesDisabled = ${scrollUpdatesDisabled};
                uiController.immediateScrollElementAtContentPointToOffset(${x}, ${y}, ${scrollX}, ${scrollY});`, resolve);
        });
    }

    static ensureVisibleContentRectUpdate()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            const visibleContentRectUpdateScript = "uiController.doAfterVisibleContentRectUpdate(() => uiController.uiScriptComplete())";
            testRunner.runUIScript(visibleContentRectUpdateScript, resolve);
        });
    }

    static longPressAndGetContextMenuContentAt(x, y)
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`
            (function() {
                uiController.didShowContextMenuCallback = function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.contentsOfUserInterfaceItem('contextMenu')));
                };
                uiController.longPressAtPoint(${x}, ${y}, function() { });
            })();`, result => resolve(JSON.parse(result)));
        });
    }

    static async setSafeAreaInsets(top, right, bottom, left)
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.setSafeAreaInsets(${top}, ${right}, ${bottom}, ${left});
                uiController.doAfterNextVisibleContentRectAndStablePresentationUpdate(function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static async setInlinePrediction(text, startIndex = 0)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.setInlinePrediction(\`${text}\`, ${startIndex})`, resolve));
    }

    static async acceptInlinePrediction()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.acceptInlinePrediction()`, resolve));
    }

    static async activateAndWaitForInputSessionAt(x, y)
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return this.activateAt(x, y);

        if (testRunner.isKeyboardImmediatelyAvailable) {
            await new Promise(resolve => {
                testRunner.runUIScript(`
                    (function() {
                        uiController.singleTapAtPoint(${x}, ${y}, function() { });
                        uiController.uiScriptComplete();
                    })()`, resolve);
            });
            await this.ensureStablePresentationUpdate();
            return;
        }

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    function clearCallbacksAndScriptComplete() {
                        uiController.didShowContextMenuCallback = null;
                        uiController.didShowKeyboardCallback = null;
                        uiController.willPresentPopoverCallback = null;
                        uiController.uiScriptComplete();
                    }
                    uiController.didShowContextMenuCallback = clearCallbacksAndScriptComplete;
                    uiController.didShowKeyboardCallback = clearCallbacksAndScriptComplete;
                    uiController.willPresentPopoverCallback = clearCallbacksAndScriptComplete;
                    uiController.singleTapAtPoint(${x}, ${y}, function() { });
                })()`, resolve);
        });
    }

    static waitForInputSessionToDismiss()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (!uiController.isShowingKeyboard && !uiController.isShowingContextMenu && !uiController.isShowingPopover) {
                        uiController.uiScriptComplete();
                        return;
                    }

                    function clearCallbacksAndScriptComplete() {
                        uiController.didHideKeyboardCallback = null;
                        uiController.didDismissPopoverCallback = null;
                        uiController.didDismissContextMenuCallback = null;
                        uiController.uiScriptComplete();
                    }

                    uiController.didHideKeyboardCallback = clearCallbacksAndScriptComplete;
                    uiController.didDismissPopoverCallback = clearCallbacksAndScriptComplete;
                    uiController.didDismissContextMenuCallback = clearCallbacksAndScriptComplete;
                })()`, resolve);
        });
    }

    static resizeWindowTo(width, height)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.resizeWindowTo(${width}, ${height})`, resolve));
    }

    static activateElementAndWaitForInputSession(element)
    {
        const x = element.offsetLeft + element.offsetWidth / 2;
        const y = element.offsetTop + element.offsetHeight / 2;
        return this.activateAndWaitForInputSessionAt(x, y);
    }

    static activateFormControl(element)
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return this.activateElement(element);

        const x = element.offsetLeft + element.offsetWidth / 2;
        const y = element.offsetTop + element.offsetHeight / 2;

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.didStartFormControlInteractionCallback = function() {
                        uiController.uiScriptComplete();
                    };
                    uiController.singleTapAtPoint(${x}, ${y}, function() { });
                })()`, resolve);
        });
    }

    static dismissFormAccessoryView()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.dismissFormAccessoryView();
                    uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static isShowingKeyboard()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("uiController.isShowingKeyboard", result => resolve(result === "true"));
        });
    }

    static isShowingFormValidationBubble()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("uiController.isShowingFormValidationBubble", result => resolve(result === "true"));
        });
    }

    static hasInputSession()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("uiController.hasInputSession", result => resolve(result === "true"));
        });
    }

    static isPresentingModally()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("uiController.isPresentingModally", result => resolve(result === "true"));
        });
    }

    static isZoomingOrScrolling()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("uiController.isZoomingOrScrolling", result => resolve(result === "true"));
        });
    }

    static async waitForZoomingOrScrollingToEnd()
    {
        do {
            await this.ensureStablePresentationUpdate();
        } while (this.isIOSFamily() && await this.isZoomingOrScrolling());
    }

    static deactivateFormControl(element)
    {
        if (!this.isWebKit2() || !this.isIOSFamily()) {
            element.blur();
            return Promise.resolve();
        }

        return new Promise(async resolve => {
            element.blur();
            while (await this.isPresentingModally())
                continue;
            while (await this.isShowingKeyboard())
                continue;
            resolve();
        });
    }

    static waitForPopoverToPresent()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (uiController.isShowingPopover)
                        uiController.uiScriptComplete();
                    else
                        uiController.willPresentPopoverCallback = () => uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static waitForPopoverToDismiss()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (uiController.isShowingPopover)
                        uiController.didDismissPopoverCallback = () => uiController.uiScriptComplete();
                    else
                        uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static waitForViewControllerToShow()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.didPresentViewControllerCallback = () => uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static waitForContextMenuToShow()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (!uiController.isShowingContextMenu)
                        uiController.didShowContextMenuCallback = () => uiController.uiScriptComplete();
                    else
                        uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static waitForContextMenuToHide()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (uiController.isShowingContextMenu)
                        uiController.didDismissContextMenuCallback = () => uiController.uiScriptComplete();
                    else
                        uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static dismissMenu()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript("uiController.dismissMenu()", resolve);
        });
    }

    static waitForKeyboardToShow()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (uiController.isShowingKeyboard)
                        uiController.uiScriptComplete();
                    else
                        uiController.didShowKeyboardCallback = () => uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static waitForKeyboardToHide()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (uiController.isShowingKeyboard)
                        uiController.didHideKeyboardCallback = () => uiController.uiScriptComplete();
                    else
                        uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static scrollbarState(scroller, isVertical)
    {
        var internalFunctions = scroller ? scroller.ownerDocument.defaultView.internals : internals;
        if (!this.isWebKit2())
            return Promise.resolve();

        if (internals.isUsingUISideCompositing() && (!scroller || scroller.nodeName != "SELECT")) {
            var scrollingNodeID = internalFunctions.scrollingNodeIDForNode(scroller);
            return new Promise(resolve => {
                testRunner.runUIScript(`(function() {
                    uiController.doAfterNextStablePresentationUpdate(function() {
                        uiController.uiScriptComplete(uiController.scrollbarStateForScrollingNodeID(${scrollingNodeID[0]}, ${scrollingNodeID[1]}, ${isVertical}));
                    });
                })()`, state => {
                    resolve(state);
                });
            });    
        } else {
            return isVertical ? Promise.resolve(internalFunctions.verticalScrollbarState(scroller)) : Promise.resolve(internalFunctions.horizontalScrollbarState(scroller));
        }
    }

    static verticalScrollbarState(scroller)
    {
        return UIHelper.scrollbarState(scroller, true);
    }

    static horizontalScrollbarState(scroller)
    {
        return UIHelper.scrollbarState(scroller, false);
    }

    static didCallEnsurePositionInformationIsUpToDateSinceLastCheck()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve(false);

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.uiScriptComplete(uiController.didCallEnsurePositionInformationIsUpToDateSinceLastCheck);
            })()`, result => {
                resolve(result === "true");
            });
        });
    }

    static clearEnsurePositionInformationIsUpToDateTracking()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.clearEnsurePositionInformationIsUpToDateTracking();
                uiController.uiScriptComplete();
            })()`, resolve);
        });
    }

    static getUICaretViewRect()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.selectionCaretViewRect));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static getUICaretViewRectInGlobalCoordinates()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.selectionCaretViewRectInGlobalCoordinates));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static getUISelectionViewRects()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.selectionRangeViewRects));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static async isSelectionVisuallyContiguous()
    {
        const rects = await this.getUISelectionViewRects();
        if (!rects?.length)
            return false;

        rects.sort((a, b) => {
            if (a.top < b.top)
                return -1;
            if (a.top > b.top)
                return 1;
            return a.left < b.left ? -1 : (a.left > b.left ? 1 : 0);
        });

        for (let i = 1; i < rects.length; ++i) {
            const previousRect = rects[i - 1];
            const rect = rects[i];

            if (previousRect.top !== rect.top)
                continue;

            if (previousRect.left + previousRect.width < rect.left)
                return false;
        }

        return true;
    }

    static async selectionBounds()
    {
        const rects = await this.getUISelectionViewRects();
        if (!rects?.length)
            return null;

        let minTop = Infinity;
        let minLeft = Infinity;
        let maxTop = -Infinity;
        let maxLeft = -Infinity;

        for (const rect of rects) {
            minTop = Math.min(minTop, rect.top);
            minLeft = Math.min(minLeft, rect.left);
            maxTop = Math.max(maxTop, rect.top + rect.height);
            maxLeft = Math.max(maxLeft, rect.left + rect.width);
        }

        return { left: minLeft, top: minTop, width: maxLeft - minLeft, height: maxTop - minTop };
    }

    static getSelectionStartGrabberViewRect()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.selectionStartGrabberViewRect));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static getSelectionEndGrabberViewRect()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.selectionEndGrabberViewRect));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static getSelectionEndGrabberViewShapePathDescription()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.selectionEndGrabberViewShapePathDescription));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static midPointOfRect(rect) {
        return { x: rect.left + (rect.width / 2), y: rect.top + (rect.height / 2) };
    }

    static rectContainsOtherRect(rect, otherRect) {
        return rect.left <= otherRect.left
            && rect.top <= otherRect.top
            && (rect.left + rect.width) >= (otherRect.left + otherRect.width)
            && (rect.top + rect.height) >= (otherRect.top + otherRect.height);
    }

    static computeLineBounds(element, firstRunIndex, lastRunIndex = undefined) {
        const range = document.createRange();
        range.selectNodeContents(element);
        const clientRects = range.getClientRects();
        const firstRect = clientRects[firstRunIndex];
        const secondRect = clientRects[lastRunIndex || firstRunIndex];
        const x = Math.min(firstRect.left, secondRect.left);
        const y = Math.min(firstRect.top, secondRect.top);
        const maxX = Math.max(firstRect.left + firstRect.width, secondRect.left + secondRect.width);
        const maxY = Math.max(firstRect.top + firstRect.height, secondRect.top + secondRect.height);
        return {
            top: Math.round(y),
            left: Math.round(x),
            width: Math.round(maxX - x),
            height: Math.round(maxY - y)
        };
    }

    static selectionCaretBackgroundColor()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("uiController.uiScriptComplete(uiController.selectionCaretBackgroundColor)", resolve);
        });
    }

    static tapHighlightViewRect()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("JSON.stringify(uiController.tapHighlightViewRect)", jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static replaceTextAtRange(text, location, length) {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.replaceTextAtRange("${text}", ${location}, ${length});
                uiController.uiScriptComplete();
            })()`, resolve);
        });
    }

    static wait(promise)
    {
        testRunner.waitUntilDone();
        if (window.finishJSTest)
            window.jsTestIsAsync = true;

        let finish = () => {
            if (window.finishJSTest)
                finishJSTest();
            else
                testRunner.notifyDone();
        }

        return promise.then(finish, finish);
    }

    static withUserGesture(callback)
    {
        internals.withUserGesture(callback);
    }

    static selectFormAccessoryPickerRow(rowIndex)
    {
        const selectRowScript = `uiController.selectFormAccessoryPickerRow(${rowIndex})`;
        return new Promise(resolve => testRunner.runUIScript(selectRowScript, resolve));
    }

    static selectFormAccessoryHasCheckedItemAtRow(rowIndex)
    {
        return new Promise(resolve => testRunner.runUIScript(`uiController.selectFormAccessoryHasCheckedItemAtRow(${rowIndex})`, result => {
            resolve(result === "true");
        }));
    }

    static selectFormPopoverTitle()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.selectFormPopoverTitle);
            })()`, resolve);
        });
    }

    static selectMenuItems()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`
            (function() {
                if (!uiController.isShowingContextMenu) {
                    uiController.didShowContextMenuCallback = function() {
                        uiController.uiScriptComplete(JSON.stringify(uiController.contentsOfUserInterfaceItem('selectMenu')));
                    };
                } else {
                    uiController.uiScriptComplete(JSON.stringify(uiController.contentsOfUserInterfaceItem('selectMenu')));
                }
            })();`, result => resolve(JSON.parse(result).selectMenu));
        });
    }

    static contextMenuItems()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(JSON.stringify(uiController.contentsOfUserInterfaceItem('contextMenu')))
            })()`, result => resolve(result ? JSON.parse(result).contextMenu : null));
        });
    }

    static setSelectedColorForColorPicker(red, green, blue)
    {
        const selectColorScript = `uiController.setSelectedColorForColorPicker(${red}, ${green}, ${blue})`;
        return new Promise(resolve => testRunner.runUIScript(selectColorScript, resolve));
    }

    static enterText(text)
    {
        const escapedText = text.replace(/`/g, "\\`");
        const enterTextScript = `(() => uiController.enterText(\`${escapedText}\`))()`;
        return new Promise(resolve => testRunner.runUIScript(enterTextScript, resolve));
    }

    static setTimePickerValue(hours, minutes)
    {
        const setValueScript = `(() => uiController.setTimePickerValue(${hours}, ${minutes}))()`;
        return new Promise(resolve => testRunner.runUIScript(setValueScript, resolve));
    }

    static timerPickerValues()
    {
        if (!this.isIOSFamily())
            return Promise.resolve();

        const uiScript = "JSON.stringify([uiController.timePickerValueHour, uiController.timePickerValueMinute])";
        return new Promise(resolve => testRunner.runUIScript(uiScript, result => {
            const [hour, minute] = JSON.parse(result)
            resolve({ hour: hour, minute: minute });
        }));
    }

    static textContentType()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.textContentType);
            })()`, resolve);
        });
    }

    static formInputLabel()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.formInputLabel);
            })()`, resolve);
        });
    }

    static dismissFilePicker()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        const script = `uiController.dismissFilePicker(() => {
            uiController.uiScriptComplete();
        })`;
        return new Promise(resolve => testRunner.runUIScript(script, resolve));
    }

    static filePickerAcceptedTypeIdentifiers()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(JSON.stringify(uiController.filePickerAcceptedTypeIdentifiers));
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static activateDataListSuggestion(index) {
        const script = `uiController.activateDataListSuggestion(${index}, () => {
            uiController.uiScriptComplete("");
        });`;
        return new Promise(resolve => testRunner.runUIScript(script, resolve));
    }

    static isShowingDataListSuggestions()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.isShowingDataListSuggestions);
            })()`, result => resolve(result === "true"));
        });
    }

    static waitForDataListSuggestionsToChangeVisibility(visible)
    {
        return new Promise(async resolve => {
            while (visible != await this.isShowingDataListSuggestions())
                continue;
            resolve();
        });
    }

    static isShowingDateTimePicker()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.isShowingDateTimePicker);
            })()`, result => resolve(result === "true"));
        });
    }

    static dateTimePickerValue()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.dateTimePickerValue);
            })()`, valueAsString => resolve(parseFloat(valueAsString)));
        });
    }

    static chooseDateTimePickerValue()
    {
        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.chooseDateTimePickerValue();
                uiController.uiScriptComplete();
            `, resolve);
        });
    }

    static zoomScale()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.zoomScale);
            })()`, scaleAsString => resolve(parseFloat(scaleAsString)));
        });
    }

    static zoomToScale(scale)
    {
        const uiScript = `uiController.zoomToScale(${scale}, () => uiController.uiScriptComplete(uiController.zoomScale))`;
        return new Promise(resolve => testRunner.runUIScript(uiScript, resolve));
    }

    static immediateZoomToScale(scale)
    {
        const uiScript = `uiController.immediateZoomToScale(${scale})`;
        return new Promise(resolve => testRunner.runUIScript(uiScript, resolve));
    }

    static async smartMagnifyAt(x, y)
    {
        if (!this.isWebKit2() || !this.isMac()) {
            console.log('Smart magnify testing is currently only supported on macOS');
            return Promise.resolve();
        }

        await UIHelper.startMonitoringWheelEvents();

        // If smartMagnify is not working, ensure you've called setWebViewAllowsMagnification(true).
        eventSender.mouseMoveTo(x, y);
        eventSender.smartMagnify();

        await UIHelper.waitForScrollCompletion();
    }

    static typeCharacter(characterString)
    {
        if (!this.isWebKit2() || !this.isIOSFamily()) {
            eventSender.keyDown(characterString);
            return;
        }

        const escapedString = characterString.replace(/\\/g, "\\\\").replace(/`/g, "\\`");
        const uiScript = `uiController.typeCharacterUsingHardwareKeyboard(\`${escapedString}\`, () => uiController.uiScriptComplete())`;
        return new Promise(resolve => testRunner.runUIScript(uiScript, resolve));
    }

    static selectWordForReplacement()
    {
        if (!this.isWebKit2())
            return;

        return new Promise(resolve => testRunner.runUIScript("uiController.selectWordForReplacement()", resolve));
    }

    static applyAutocorrection(newText, oldText, underline)
    {
        if (!this.isWebKit2())
            return;

        const [escapedNewText, escapedOldText] = [newText.replace(/`/g, "\\`"), oldText.replace(/`/g, "\\`")];
        const uiScript = `uiController.applyAutocorrection(\`${escapedNewText}\`, \`${escapedOldText}\`, () => uiController.uiScriptComplete(), ${underline})`;
        return new Promise(resolve => testRunner.runUIScript(uiScript, resolve));
    }

    static inputViewBounds()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(JSON.stringify(uiController.inputViewBounds));
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static calendarType()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.doAfterNextStablePresentationUpdate(() => {
                    uiController.uiScriptComplete(JSON.stringify(uiController.calendarType));
                })
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static setDefaultCalendarType(calendarIdentifier, localeIdentifier)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.setDefaultCalendarType('${calendarIdentifier}', '${localeIdentifier}')`, resolve));

    }

    static setViewScale(scale)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.setViewScale(${scale})`, resolve));
    }

    static setScrollViewKeyboardAvoidanceEnabled(enabled)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.setScrollViewKeyboardAvoidanceEnabled(${enabled})`, resolve));
    }

    static async setAlwaysBounceVertical(enabled)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.setAlwaysBounceVertical(${enabled})`, resolve));
    }

    static async setAlwaysBounceHorizontal(enabled)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.setAlwaysBounceHorizontal(${enabled})`, resolve));
    }

    static presentFindNavigator() {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.presentFindNavigator()`, resolve));
    }

    static dismissFindNavigator() {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.dismissFindNavigator()`, resolve));
    }

    static resignFirstResponder()
    {
        if (!this.isWebKit2()) {
            testRunner.setMainFrameIsFirstResponder(false);
            return Promise.resolve();
        }

        return new Promise(resolve => testRunner.runUIScript(`uiController.resignFirstResponder()`, resolve));
    }

    static becomeFirstResponder()
    {
        if (!this.isWebKit2()) {
            testRunner.setMainFrameIsFirstResponder(true);
            return Promise.resolve();
        }

        return new Promise(resolve => testRunner.runUIScript(`uiController.becomeFirstResponder()`, resolve));
    }

    static removeViewFromWindow()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        const scriptToRun = `(function() {
            uiController.removeViewFromWindow();
            uiController.uiScriptComplete();
        })()`;
        return new Promise(resolve => testRunner.runUIScript(scriptToRun, resolve));
    }

    static addViewToWindow()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        const scriptToRun = `(function() {
            uiController.addViewToWindow();
            uiController.uiScriptComplete();
        })()`;
        return new Promise(resolve => testRunner.runUIScript(scriptToRun, resolve));
    }

    static minimumZoomScale()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.minimumZoomScale);
            })()`, scaleAsString => resolve(parseFloat(scaleAsString)))
        });
    }

    static stylusTapAt(x, y, modifiers=[])
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.stylusTapAtPointWithModifiers(${x}, ${y}, 2, 1, 0.5, ${JSON.stringify(modifiers)}, function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static attachmentInfo(attachmentIdentifier)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(JSON.stringify(uiController.attachmentInfo('${attachmentIdentifier}')));
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            })
        });
    }

    static insertAttachmentForFilePath(path, contentType)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.insertAttachmentForFilePath('${path}', '${contentType}', function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static setMinimumEffectiveWidth(effectiveWidth)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.setMinimumEffectiveWidth(${effectiveWidth})`, resolve));
    }

    static setAllowsViewportShrinkToFit(allows)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.setAllowsViewportShrinkToFit(${allows})`, resolve));
    }

    static setKeyboardInputModeIdentifier(identifier)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        const escapedIdentifier = identifier.replace(/`/g, "\\`");
        return new Promise(resolve => testRunner.runUIScript(`uiController.setKeyboardInputModeIdentifier(\`${escapedIdentifier}\`)`, resolve));
    }

    static setFocusStartsInputSessionPolicy(policy)
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.setFocusStartsInputSessionPolicy("${policy}")`, resolve));
    }

    static contentOffset()
    {
        if (!this.isIOSFamily())
            return Promise.resolve();

        const uiScript = "JSON.stringify([uiController.contentOffsetX, uiController.contentOffsetY])";
        return new Promise(resolve => testRunner.runUIScript(uiScript, result => {
            const [offsetX, offsetY] = JSON.parse(result)
            resolve({ x: offsetX, y: offsetY });
        }));
    }

    static adjustedContentInset()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript("JSON.stringify(uiController.adjustedContentInset)", result => resolve(JSON.parse(result)));
        });
    }

    static undoAndRedoLabels()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        const script = "JSON.stringify([uiController.lastUndoLabel, uiController.firstRedoLabel])";
        return new Promise(resolve => testRunner.runUIScript(script, result => resolve(JSON.parse(result))));
    }

    static waitForMenuToShow()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (!uiController.isShowingMenu)
                        uiController.didShowMenuCallback = () => uiController.uiScriptComplete();
                    else
                        uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static waitForMenuToHide()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (uiController.isShowingMenu)
                        uiController.didHideMenuCallback = () => uiController.uiScriptComplete();
                    else
                        uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static isShowingMenu()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`uiController.isShowingMenu`, result => resolve(result === "true"));
        });
    }

    static isDismissingMenu()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`uiController.isDismissingMenu`, result => resolve(result === "true"));
        });
    }

    static menuRect()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("JSON.stringify(uiController.menuRect)", result => resolve(JSON.parse(result)));
        });
    }

    static contextMenuRect()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("JSON.stringify(uiController.contextMenuRect)", result => resolve(JSON.parse(result)));
        });
    }

    static contextMenuPreviewRect()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("JSON.stringify(uiController.contextMenuPreviewRect)", result => resolve(JSON.parse(result)));
        });
    }

    static setHardwareKeyboardAttached(attached)
    {
        return new Promise(resolve => testRunner.runUIScript(`uiController.setHardwareKeyboardAttached(${attached ? "true" : "false"})`, resolve));
    }

    static setWebViewEditable(editable)
    {
        return new Promise(resolve => testRunner.runUIScript(`uiController.setWebViewEditable(${editable ? "true" : "false"})`, resolve));
    }

    static setWebViewAllowsMagnification(allowsMagnification)
    {
        return new Promise(resolve => testRunner.runUIScript(`uiController.setWebViewAllowsMagnification(${allowsMagnification ? "true" : "false"})`, resolve));
    }

    static rectForMenuAction(action)
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`
                (() => {
                    const rect = uiController.rectForMenuAction("${action}");
                    uiController.uiScriptComplete(rect ? JSON.stringify(rect) : "");
                })();
            `, stringResult => {
                resolve(stringResult.length ? JSON.parse(stringResult) : null);
            });
        });
    }

    static chooseMenuAction(action)
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`
                (() => {
                    uiController.chooseMenuAction("${action}", () => {
                        uiController.uiScriptComplete();
                    });
                })();
            `, resolve);
        });
    }

    static waitForEvent(target, eventName)
    {
        return new Promise(resolve => target.addEventListener(eventName, resolve, { once: true }));
    }

    static waitForEventHandler(target, eventName, handler)
    {
        return new Promise((resolve) => {
            target.addEventListener(eventName, (e) => {
                handler(e)
                resolve();
            }, { once: true });
        });
    }

    static callFunctionAndWaitForEvent(functionToCall, target, eventName)
    {
        return new Promise(async resolve => {
            let event;
            await Promise.all([
                new Promise((eventListenerResolve) => {
                    target.addEventListener(eventName, (e) => {
                        event = e;
                        eventListenerResolve();
                    }, { once: true });
                }),
                new Promise(async functionResolve => {
                    await functionToCall();
                    functionResolve();
                })
            ]);
            resolve(event);
        });
    }

    static callFunctionAndWaitForScrollToFinish(functionToCall, ...theArguments)
    {
        return UIHelper.callFunctionAndWaitForTargetScrollToFinish(window, functionToCall, theArguments)
    }

    static callFunctionAndWaitForTargetScrollToFinish(scrollTarget, functionToCall, ...theArguments)
    {
        return new Promise((resolved) => {
            function scrollDidFinish() {
                scrollTarget.removeEventListener("scroll", handleScroll, true);
                resolved();
            }

            let lastScrollTimerId = 0; // When the timer with this id fires then the page has finished scrolling.
            function handleScroll() {
                if (lastScrollTimerId) {
                    window.clearTimeout(lastScrollTimerId);
                    lastScrollTimerId = 0;
                }
                lastScrollTimerId = window.setTimeout(scrollDidFinish, 300); // Over 250ms to give some room for error.
            }
            scrollTarget.addEventListener("scroll", handleScroll, true);

            functionToCall.apply(this, theArguments);
        });
    }

    static waitForTargetScrollAnimationToSettle(scrollTarget)
    {
        return new Promise((resolved) => {
            let lastObservedScrollPosition = [scrollTarget.scrollLeft, scrollTarget.scrollTop];
            let frameOfLastChange = 0;
            let totalFrames = 0;

            function animationFrame() {
                if (lastObservedScrollPosition[0] != scrollTarget.scrollLeft ||
                    lastObservedScrollPosition[1] != scrollTarget.scrollTop) {
                    lastObservedScrollPosition = [scrollTarget.scrollLeft, scrollTarget.scrollTop];
                    frameOfLastChange = totalFrames;
                }

                // If we have gone 20 frames without changing, resolve. If we have gone 500, then time out.
                // This matches the amount of frames used in the WPT scroll animation helper.
                if (totalFrames - frameOfLastChange >= 20 || totalFrames > 500)
                    resolved();

                totalFrames++;
                requestAnimationFrame(animationFrame);
            }

            requestAnimationFrame(animationFrame);
        });
    }

    static beginInteractiveObscuredInsetsChange()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript("uiController.beginInteractiveObscuredInsetsChange()", resolve);
        });
    }

    static endInteractiveObscuredInsetsChange()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript("uiController.endInteractiveObscuredInsetsChange()", resolve);
        });
    }

    static async setObscuredInsets(top, right, bottom, left)
    {
        if (!window.testRunner)
            return Promise.resolve();

        if (this.isWebKit2() && this.isIOSFamily()) {
            const scriptToRun = `uiController.setObscuredInsets(${top}, ${right}, ${bottom}, ${left})`;
            await new Promise(resolve => testRunner.runUIScript(scriptToRun, resolve));
            await this.ensureVisibleContentRectUpdate();
        } else
            testRunner.setObscuredContentInsets(top, right, bottom, left);
        await this.ensurePresentationUpdate();
    }

    static rotateDevice(orientationName, animatedResize = false)
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.${animatedResize ? "simulateRotationLikeSafari" : "simulateRotation"}("${orientationName}", function() {
                    uiController.doAfterVisibleContentRectUpdate(() => uiController.uiScriptComplete());
                });
            })()`, resolve);
        });
    }

    static getScrollingTree()
    {
        if (!this.isWebKit2())
            return Promise.resolve('');

        if (window.internals.haveScrollingTree())
            return Promise.resolve(window.internals.scrollingTreeAsText());
            
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                return uiController.scrollingTreeAsText;
            })()`, resolve);
        });
    }

    static getUIViewTree()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                return uiController.uiViewTreeAsText;
            })()`, resolve);
        });
    }

    static getUIViewTreeForCompositedElement(element)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        const layerID = window.internals?.layerIDForElement(element);
        const script = `uiController.uiScriptComplete(uiController.uiViewTreeAsTextForViewWithLayerID(${layerID}))`;
        return new Promise(resolve => {
            testRunner.runUIScript(script, resolve);
        });
    }

    static propertiesOfLayerWithID(layerID)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        const script = `uiController.uiScriptComplete(JSON.stringify(uiController.propertiesOfLayerWithID(${layerID})))`;
        return new Promise(resolve => {
            testRunner.runUIScript(script, properties => resolve(JSON.parse(properties)));
        });
    }

    static getCALayerTree()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                return uiController.caLayerTreeAsText;
            })()`, resolve);
        });
    }

    static getCALayerTreeForCompositedElement(element)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        const layerID = window.internals?.layerIDForElement(element);
        const script = `uiController.uiScriptComplete(uiController.caLayerTreeAsTextForLayerWithID(${layerID}))`;
        return new Promise(resolve => {
            testRunner.runUIScript(script, resolve);
        });
    }

    static remoteAnimationStackForElement(element)
    {
        if (!this.isWebKit2() || !element)
            return Promise.resolve();

        const layerID = window.internals?.layerIDForElement(element);
        const script = `uiController.uiScriptComplete(uiController.animationStackForLayerWithID(${layerID}))`;
        return new Promise(resolve => {
            testRunner.runUIScript(script, result => resolve(JSON.parse(result)));
        });
    }

    static dragFromPointToPoint(fromX, fromY, toX, toY, duration)
    {
        if (!this.isWebKit2() || !this.isIOSFamily()) {
            eventSender.mouseMoveTo(fromX, fromY);
            eventSender.mouseDown();
            eventSender.leapForward(duration * 1000);
            eventSender.mouseMoveTo(toX, toY);
            eventSender.mouseUp();
            return Promise.resolve();
        }

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.dragFromPointToPoint(${fromX}, ${fromY}, ${toX}, ${toY}, ${duration}, () => {
                    uiController.uiScriptComplete();
                });
            })();`, resolve);
        });
    }

    static async pinch(firstStartX, firstStartY, secondStartX, secondStartY, firstEndX, firstEndY, secondEndX, secondEndY)
    {
        await UIHelper.sendEventStream({
            events: [
                {
                    interpolate : "linear",
                    timestep : 0.01,
                    coordinateSpace : "content",
                    startEvent : {
                        inputType : "hand",
                        timeOffset : 0,
                        touches : [
                            { inputType : "finger", phase : "began", id : 1, x : firstStartX, y : firstStartY, pressure : 0 },
                            { inputType : "finger", phase : "began", id : 2, x : secondStartX, y : secondStartY, pressure : 0 }
                        ]
                    },
                    endEvent : {
                        inputType : "hand",
                        timeOffset : 0.01,
                        touches : [
                            { inputType : "finger", phase : "began", id : 1, x : firstStartX, y : firstStartY, pressure : 0 },
                            { inputType : "finger", phase : "began", id : 2, x : secondStartX, y : secondStartY, pressure : 0 }
                        ]
                    }
                },
                {
                    interpolate : "linear",
                    timestep : 0.01,
                    coordinateSpace : "content",
                    startEvent : {
                        inputType : "hand",
                        timeOffset : 0.01,
                        touches : [
                            { inputType : "finger", phase : "moved", id : 1, x : firstStartX, y : firstStartY, pressure : 0 },
                            { inputType : "finger", phase : "moved", id : 2, x : secondStartX, y : secondStartY, pressure : 0 }
                        ]
                    },
                    endEvent : {
                        inputType : "hand",
                        timeOffset : 0.9,
                        touches : [
                            { inputType : "finger", phase : "moved", id : 1, x : firstEndX, y : firstEndY, pressure : 0 },
                            { inputType : "finger", phase : "moved", id : 2, x : secondEndX, y : secondEndY, pressure : 0 }
                        ]
                    }
                },
                {
                    interpolate : "linear",
                    timestep : 0.01,
                    coordinateSpace : "content",
                    startEvent : {
                        inputType : "hand",
                        timeOffset : 0.9,
                        touches : [
                            { inputType : "finger", phase : "stationary", id : 1, x : firstEndX, y : firstEndY, pressure : 0 },
                            { inputType : "finger", phase : "stationary", id : 2, x : secondEndX, y : secondEndY, pressure : 0 }
                        ]
                    },
                    endEvent : {
                        inputType : "hand",
                        timeOffset : 0.99,
                        touches : [
                            { inputType : "finger", phase : "stationary", id : 1, x : firstEndX, y : firstEndY, pressure : 0 },
                            { inputType : "finger", phase : "stationary", id : 2, x : secondEndX, y : secondEndY, pressure : 0 }
                        ]
                    }
                },
                {
                    interpolate : "linear",
                    timestep : 0.01,
                    coordinateSpace : "content",
                    startEvent : {
                        inputType : "hand",
                        timeOffset : 0.99,
                        touches : [
                            { inputType : "finger", phase : "ended", id : 1, x : firstEndX, y : firstEndY, pressure : 0 },
                            { inputType : "finger", phase : "ended", id : 2, x : secondEndX, y : secondEndY, pressure : 0 }
                        ]
                    },
                    endEvent : {
                        inputType : "hand",
                        timeOffset : 1,
                        touches : [
                            { inputType : "finger", phase : "ended", id : 1, x : firstEndX, y : firstEndY, pressure : 0 },
                            { inputType : "finger", phase : "ended", id : 2, x : secondEndX, y : secondEndY, pressure : 0 }
                        ]
                    }
                }
            ]
        });
    }

    static setWindowIsKey(isKey)
    {
        if (!this.isWebKit2()) {
            testRunner.setWindowIsKey(isKey);
            return Promise.resolve();
        }
        const script = `uiController.windowIsKey = ${isKey}`;
        return new Promise(resolve => testRunner.runUIScript(script, resolve));
    }

    static windowIsKey()
    {
        const script = "uiController.uiScriptComplete(uiController.windowIsKey)";
        return new Promise(resolve => testRunner.runUIScript(script, (result) => {
            resolve(result === "true");
        }));
    }

    static waitForDoubleTapDelay()
    {
        const uiScript = `uiController.doAfterDoubleTapDelay(() => uiController.uiScriptComplete(""))`;
        return new Promise(resolve => testRunner.runUIScript(uiScript, resolve));
    }

    static async waitForSelectionToAppear() {
        while (true) {
            let selectionRects = await this.getUISelectionViewRects();
            if (selectionRects.length > 0)
                return selectionRects;
        }
    }

    static async waitForSelectionToDisappear() {
        while (true) {
            if (!(await this.getUISelectionViewRects()).length)
                break;
        }
    }

    static async copyText(text) {
        const copyTextScript = `uiController.copyText(\`${text.replace(/`/g, "\\`")}\`)`;
        return new Promise(resolve => testRunner.runUIScript(copyTextScript, resolve));
    }

    static async paste() {
        return new Promise(resolve => testRunner.runUIScript(`uiController.paste()`, resolve));
    }

    static async pasteboardChangeCount() {
        return new Promise(resolve => testRunner.runUIScript(`uiController.pasteboardChangeCount`, (result) => {
            resolve(parseInt(result))
        }));
    }

    static async setContinuousSpellCheckingEnabled(enabled) {
        return new Promise(resolve => {
            testRunner.runUIScript(`uiController.setContinuousSpellCheckingEnabled(${enabled})`, resolve);
        });
    }

    static async longPressElement(element)
    {
        const { x, y } = this.midPointOfRect(element.getBoundingClientRect());
        return this.longPressAtPoint(x, y);
    }

    static async longPressAtPoint(x, y)
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.longPressAtPoint(${x}, ${y}, function() {
                        uiController.uiScriptComplete();
                    });
                })();`, resolve);
        });
    }

    static async setSpellCheckerResults(results)
    {
        if (!this.isMac() && !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.setSpellCheckerResults(${JSON.stringify(results)});
                uiController.uiScriptComplete();
            })()`, resolve);
        });
    }

    static async activateElementAfterInstallingTapGestureOnWindow(element)
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return activateElement(element);

        const x = element.offsetLeft + element.offsetWidth / 2;
        const y = element.offsetTop + element.offsetHeight / 2;
        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    let progress = 0;
                    function incrementProgress() {
                        if (++progress == 2)
                            uiController.uiScriptComplete();
                    }
                    uiController.installTapGestureOnWindow(incrementProgress);
                    uiController.singleTapAtPoint(${x}, ${y}, incrementProgress);
                })();`, resolve);
        });
    }

    static mayContainEditableElementsInRect(x, y, width, height)
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve(false);

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.doAfterPresentationUpdate(function() {
                        uiController.uiScriptComplete(uiController.mayContainEditableElementsInRect(${x}, ${y}, ${width}, ${height}));
                    })
                })();`, result => resolve(result === "true"));
        });
    }

    static currentImageAnalysisRequestID()
    {
        if (!this.isWebKit2())
            return Promise.resolve(0);

        return new Promise(resolve => {
            testRunner.runUIScript("uiController.uiScriptComplete(uiController.currentImageAnalysisRequestID)", result => resolve(result));
        });
    }

    static installFakeMachineReadableCodeResultsForImageAnalysis()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript("uiController.installFakeMachineReadableCodeResultsForImageAnalysis()", resolve);
        });
    }

    static moveToNextByKeyboardAccessoryBar()
    {
        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.keyboardAccessoryBarNext();
                uiController.uiScriptComplete();
            `, resolve);
        });
    }

    static moveToPreviousByKeyboardAccessoryBar()
    {
        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.keyboardAccessoryBarPrevious();
                uiController.uiScriptComplete();
            `, resolve);
        });
    }

    static waitForContactPickerToShow()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (!uiController.isShowingContactPicker)
                        uiController.didShowContactPickerCallback = () => uiController.uiScriptComplete();
                    else
                        uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static waitForContactPickerToHide()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (uiController.isShowingContactPicker)
                        uiController.didHideContactPickerCallback = () => uiController.uiScriptComplete();
                    else
                        uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static dismissContactPickerWithContacts(contacts)
    {
       const script = `(() => uiController.dismissContactPickerWithContacts(${JSON.stringify(contacts)}))()`;
       return new Promise(resolve => testRunner.runUIScript(script, resolve));
    }

    static setAppAccentColor(red, green, blue)
    {
        if (!this.isWebKit2() || !this.isMac())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.setAppAccentColor(${red}, ${green}, ${blue});
            })()`, resolve);
        });
    }

    static async cookiesForDomain(domain)
    {
        if (!this.isWebKit2())
            return Promise.resolve([ ]);

        const script = `uiController.cookiesForDomain("${domain}", cookieData => {
            uiController.uiScriptComplete(JSON.stringify(cookieData));
        });`;

        return new Promise(resolve => {
            testRunner.runUIScript(script, cookieData => {
                resolve(JSON.parse(cookieData));
            });
        });
    }

    static cancelFixedColorExtensionFadeAnimations()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        const script = "uiController.cancelFixedColorExtensionFadeAnimations()";
        return new Promise(resolve => testRunner.runUIScript(script, resolve));
    }

    static fixedContainerEdgeColors()
    {
        if (!this.isWebKit2())
            return { };

        const script = "JSON.stringify(uiController.fixedContainerEdgeColors)";
        return new Promise(resolve => testRunner.runUIScript(script, colors => {
            resolve(JSON.parse(colors));
        }));
    }

    static async waitForFixedContainerEdgeColors(expectedColors)
    {
        while (true) {
            await this.renderingUpdate();
            const colors = await this.fixedContainerEdgeColors();
            for (const edge of ["top", "left", "right", "bottom"]) {
                if (colors[edge] !== expectedColors[edge])
                    continue;
            }
            break;
        }
    }

    static addChromeInputField()
    {
        return new Promise(resolve => testRunner.addChromeInputField(resolve));
    }

    static removeChromeInputField()
    {
        return new Promise(resolve => testRunner.removeChromeInputField(resolve));
    }

    static setTextInChromeInputField(text)
    {
        return new Promise(resolve => testRunner.setTextInChromeInputField(text, resolve));
    }

    static selectChromeInputField()
    {
        return new Promise(resolve => testRunner.selectChromeInputField(resolve));
    }

    static getSelectedTextInChromeInputField()
    {
        return new Promise(resolve => testRunner.getSelectedTextInChromeInputField(resolve));
    }

    static requestTextExtraction(options)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.requestTextExtraction(
                    result => uiController.uiScriptComplete(result),
                    ${JSON.stringify(options)}
                );
            })()`, resolve);
        });
    }

    static requestRenderedTextForFrontmostTarget(x, y)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.requestRenderedTextForFrontmostTarget(${x}, ${y}, result => uiController.uiScriptComplete(result));
            })()`, resolve);
        });
    }

    static adjustVisibilityForFrontmostTarget(x, y) {
        if (!this.isWebKit2())
            return Promise.resolve();

        if (x instanceof HTMLElement) {
            const point = this.midPointOfRect(x.getBoundingClientRect());
            x = point.x;
            y = point.y;
        }

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.adjustVisibilityForFrontmostTarget(${x}, ${y}, result => uiController.uiScriptComplete(result || ""));
            })()`, resolve);
        });
    }

    static resetVisibilityAdjustments() {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript("uiController.resetVisibilityAdjustments(result => uiController.uiScriptComplete(result));", resolve);
        });
    }

    static async waitForPDFFadeIn()
    {
        const pdfFadeInDelay = 250;
        await new Promise(resolve => setTimeout(resolve, pdfFadeInDelay));
        await new Promise(requestAnimationFrame);
    }

    static async frontmostViewAtPoint(x, y) {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`uiController.frontmostViewAtPoint(${x}, ${y})`, resolve);
        });
    }

    static async keyboardUpdateForChangedSelectionCount() {
        if (!this.isWebKit2())
            return 0;

        return new Promise(resolve => {
            testRunner.runUIScript("uiController.keyboardUpdateForChangedSelectionCount", resolve);
        });
    }

    static async typeCharacters(stringToType, waitForEvent = "input", eventTarget = null) {
        for (let character of [...stringToType]) {
            await UIHelper.callFunctionAndWaitForEvent(async () => {
                if (window.testRunner)
                    await UIHelper.typeCharacter(character);
                await UIHelper.ensurePresentationUpdate();
            }, eventTarget || document.activeElement, waitForEvent);
        }
    }

    static async requestDebugText(options = { })
    {
        if (!this.isWebKit2())
            return Promise.resolve("");

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.requestDebugText(result => {
                    uiController.uiScriptComplete(result);
                }, ${JSON.stringify(options)});
            })()`, debugText => {
                if (options.normalize) {
                    debugText = debugText
                        .replace(/uid=\d+/g, "uid=…")
                        .replace(/\[\d+,\d+;\d+x\d+\]/g, "[…]")
                        .replace(/\t/g, "    ");
                }
                resolve(debugText);
            });
        });
    }

    static nodeIdentifierFromDebugText(text, searchTerm)
    {
        for (let line of text.split("\n")) {
            if (!line.includes(searchTerm))
                continue;

            const match = line.match(/uid=(\d+)/);
            if (match)
                return match[1];
        }
        return null;
    }

    static async performTextExtractionInteraction(action, options)
    {
        if (!this.isWebKit2())
            return Promise.resolve(false);

        return new Promise(resolve => {
            const scriptToRun = `uiController.performTextExtractionInteraction("${action}", ${JSON.stringify(options)}, result => {
                uiController.uiScriptComplete(result)
            })`;
            testRunner.runUIScript(scriptToRun, resolve);
        });
    }
}

UIHelper.EventStreamBuilder = class {
    constructor()
    {
        // FIXME: This could support additional customization options, such as interpolation, timestep, and different
        // digitizer indices in the future. For now, just make it simpler to string together sequences of pan gestures.
        this._reset();
    }

    _reset() {
        this.events = [];
        this.currentTimeOffset = 0;
        this.currentX = 0;
        this.currentY = 0;
    }

    begin(x, y) {
        console.assert(this.currentTimeOffset === 0);
        this.events.push({
            interpolate : "linear",
            timestep : 0.016,
            coordinateSpace : "content",
            startEvent : {
                inputType : "hand",
                timeOffset : this.currentTimeOffset,
                touches : [{ inputType : "finger", phase : "began", id : 1, x : x, y : y, pressure : 0 }]
            },
            endEvent : {
                inputType : "hand",
                timeOffset : this.currentTimeOffset,
                touches : [{ inputType : "finger", phase : "began", id : 1, x : x, y : y, pressure : 0 }]
            }
        });
        this.currentX = x;
        this.currentY = y;
        return this;
    }

    wait(duration) {
        this.currentTimeOffset += duration;
        return this;
    }

    move(x, y, duration = 0) {
        const previousTimeOffset = this.currentTimeOffset;
        this.currentTimeOffset += duration;
        this.events.push({
            interpolate : "linear",
            timestep : 0.016,
            coordinateSpace : "content",
            startEvent : {
                inputType : "hand",
                timeOffset : previousTimeOffset,
                touches : [{ inputType : "finger", phase : "moved", id : 1, x : this.currentX, y : this.currentY, pressure : 0 }]
            },
            endEvent : {
                inputType : "hand",
                timeOffset : this.currentTimeOffset,
                touches : [{ inputType : "finger", phase : "moved", id : 1, x : x, y : y, pressure : 0 }]
            }
        });
        this.currentX = x;
        this.currentY = y;
        return this;
    }

    end() {
        this.events.push({
            interpolate : "linear",
            timestep : 0.016,
            coordinateSpace : "content",
            startEvent : {
                inputType : "hand",
                timeOffset : this.currentTimeOffset,
                touches : [{ inputType : "finger", phase : "ended", id : 1, x : this.currentX, y : this.currentY, pressure : 0 }]
            },
            endEvent : {
                inputType : "hand",
                timeOffset : this.currentTimeOffset,
                touches : [{ inputType : "finger", phase : "ended", id : 1, x : this.currentX, y : this.currentY, pressure : 0 }]
            }
        });
        return this;
    }

    takeResult() {
        const events = this.events;
        this._reset();
        return { "events": events };
    }
}
