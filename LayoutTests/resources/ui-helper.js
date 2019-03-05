
window.UIHelper = class UIHelper {
    static isIOS()
    {
        return navigator.userAgent.includes('iPhone') || navigator.userAgent.includes('iPad');
    }

    static isWebKit2()
    {
        return window.testRunner.isWebKit2;
    }

    static doubleClickAt(x, y)
    {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
        eventSender.mouseDown();
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

    static tapAt(x, y, modifiers=[])
    {
        console.assert(this.isIOS());

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

    static doubleTapAt(x, y)
    {
        console.assert(this.isIOS());

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
                uiController.doubleTapAtPoint(${x}, ${y}, function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static zoomByDoubleTappingAt(x, y)
    {
        console.assert(this.isIOS());

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
                uiController.doubleTapAtPoint(${x}, ${y}, () => {});`, resolve);
        });
    }

    static activateAt(x, y)
    {
        if (!this.isWebKit2() || !this.isIOS()) {
            eventSender.mouseMoveTo(x, y);
            eventSender.mouseDown();
            eventSender.mouseUp();
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.singleTapAtPoint(${x}, ${y}, function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    static activateElement(element)
    {
        const x = element.offsetLeft + element.offsetWidth / 2;
        const y = element.offsetTop + element.offsetHeight / 2;
        return UIHelper.activateAt(x, y);
    }

    static async doubleActivateAt(x, y)
    {
        if (this.isIOS())
            await UIHelper.doubleTapAt(x, y);
        else
            await UIHelper.doubleClickAt(x, y);
    }

    static async doubleActivateAtSelectionStart()
    {
        const rects = window.getSelection().getRangeAt(0).getClientRects();
        const x = rects[0].left;
        const y = rects[0].top;
        if (this.isIOS()) {
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
        if (this.isIOS()) {
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
        if (!this.isWebKit2() || !this.isIOS()) {
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

    static ensureVisibleContentRectUpdate()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            const visibleContentRectUpdateScript = "uiController.doAfterVisibleContentRectUpdate(() => uiController.uiScriptComplete())";
            testRunner.runUIScript(visibleContentRectUpdateScript, resolve);
        });
    }

    static activateAndWaitForInputSessionAt(x, y)
    {
        if (!this.isWebKit2() || !this.isIOS())
            return this.activateAt(x, y);

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.didShowKeyboardCallback = function() {
                        uiController.uiScriptComplete();
                    };
                    uiController.singleTapAtPoint(${x}, ${y}, function() { });
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
        if (!this.isWebKit2() || !this.isIOS())
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

    static isShowingKeyboard()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("uiController.isShowingKeyboard", result => resolve(result === "true"));
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
        if (!this.isWebKit2() || !this.isIOS()) {
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

    static waitForKeyboardToHide()
    {
        if (!this.isWebKit2() || !this.isIOS())
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
        if (!this.isWebKit2() || !this.isIOS())
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
        if (!this.isWebKit2() || !this.isIOS())
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
        if (!this.isWebKit2() || !this.isIOS())
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
        if (!this.isWebKit2() || !this.isIOS())
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
        if (!this.isWebKit2() || !this.isIOS())
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
        if (!this.isWebKit2() || !this.isIOS())
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
        const selectRowScript = `(() => uiController.selectFormAccessoryPickerRow(${rowIndex}))()`;
        return new Promise(resolve => testRunner.runUIScript(selectRowScript, resolve));
    }

    static selectFormPopoverTitle()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.selectFormPopoverTitle);
            })()`, resolve);
        });
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

    static setShareSheetCompletesImmediatelyWithResolution(resolved)
    {
        const resolveShareSheet = `(() => uiController.setShareSheetCompletesImmediatelyWithResolution(${resolved}))()`;
        return new Promise(resolve => testRunner.runUIScript(resolveShareSheet, resolve));
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

    static isShowingDataListSuggestions()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.isShowingDataListSuggestions);
            })()`, result => resolve(result === "true"));
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

    static typeCharacter(characterString)
    {
        if (!this.isWebKit2() || !this.isIOS()) {
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
        if (!this.isWebKit2() || !this.isIOS())
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

    static setDefaultCalendarType(calendarIdentifier)
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.setDefaultCalendarType('${calendarIdentifier}')`, resolve));

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

    static drawSquareInEditableImage()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => testRunner.runUIScript(`uiController.drawSquareInEditableImage()`, resolve));
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

    static numberOfStrokesInEditableImage()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.numberOfStrokesInEditableImage);
            })()`, numberAsString => resolve(parseInt(numberAsString, 10)))
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
        if (!this.isIOS())
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

    static menuRect()
    {
        return new Promise(resolve => {
            testRunner.runUIScript("JSON.stringify(uiController.menuRect)", result => resolve(JSON.parse(result)));
        });
    }
}
