
window.UIHelper = class UIHelper {
    static isIOSFamily()
    {
        return testRunner.isIOSFamily;
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

        eventSender.monitorWheelEvents();
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseScrollByWithWheelAndMomentumPhases(beginX, beginY, "began", "none");
        eventSender.mouseScrollByWithWheelAndMomentumPhases(deltaX, deltaY, "changed", "none");
        eventSender.mouseScrollByWithWheelAndMomentumPhases(0, 0, "ended", "none");
        return new Promise(resolve => {
            eventSender.callAfterScrollingCompletes(() => {
                requestAnimationFrame(resolve);
            });
        });
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

    static async waitForScrollCompletion()
    {
        return new Promise(resolve => {
            eventSender.callAfterScrollingCompletes(() => {
                requestAnimationFrame(resolve);
            });
        });
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

    static toggleCapsLock()
    {
        return new Promise((resolve) => {
            testRunner.runUIScript(`uiController.toggleCapsLock(() => uiController.uiScriptComplete());`, resolve);
        });
    }

    static keyboardIsAutomaticallyShifted()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`uiController.keyboardIsAutomaticallyShifted`, result => resolve(result === "true"));
        });
    }

    static ensurePresentationUpdate()
    {
        if (!this.isWebKit2()) {
            testRunner.display();
            return Promise.resolve();
        }

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.doAfterPresentationUpdate(function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static ensureStablePresentationUpdate()
    {
        if (!this.isWebKit2()) {
            testRunner.display();
            return Promise.resolve();
        }

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

    static activateAndWaitForInputSessionAt(x, y)
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return this.activateAt(x, y);

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

    static isShowingPopover()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("uiController.isShowingPopover", result => resolve(result === "true"));
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

    static getUICaretRect()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.textSelectionCaretRect));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static getUISelectionRects()
    {
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.textSelectionRangeRects));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
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

    static applyAutocorrection(newText, oldText)
    {
        if (!this.isWebKit2())
            return;

        const [escapedNewText, escapedOldText] = [newText.replace(/`/g, "\\`"), oldText.replace(/`/g, "\\`")];
        const uiScript = `uiController.applyAutocorrection(\`${escapedNewText}\`, \`${escapedOldText}\`, () => uiController.uiScriptComplete())`;
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

    static resignFirstResponder()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.resignFirstResponder()`, resolve));
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

    static setHardwareKeyboardAttached(attached)
    {
        return new Promise(resolve => testRunner.runUIScript(`uiController.setHardwareKeyboardAttached(${attached ? "true" : "false"})`, resolve));
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

    static async chooseMenuAction(action)
    {
        const menuRect = await this.rectForMenuAction(action);
        if (menuRect)
            await this.activateAt(menuRect.left + menuRect.width / 2, menuRect.top + menuRect.height / 2);
    }

    static waitForEvent(target, eventName)
    {
        return new Promise(resolve => target.addEventListener(eventName, resolve, { once: true }));
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
                    }, {once: true});
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
        if (!this.isWebKit2() || !this.isIOSFamily())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                return uiController.scrollingTreeAsText;
            })()`, resolve);
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

    static setWindowIsKey(isKey)
    {
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

    static async setContinuousSpellCheckingEnabled(enabled) {
        return new Promise(resolve => {
            testRunner.runUIScript(`uiController.setContinuousSpellCheckingEnabled(${enabled})`, resolve);
        });
    }

    static async longPressElement(element)
    {
        return this.longPressAtPoint(element.offsetLeft + element.offsetWidth / 2, element.offsetTop + element.offsetHeight / 2);
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

    static moveToNextByKeyboardAccessoryBar()
    {
        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.keyboardAccessoryBarNext();
                uiController.uiScriptComplete();
            `, resolve);
        });
    }

    static moveToPrevByKeyboardAccessoryBar()
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
