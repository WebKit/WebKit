if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

let currentTest;
let numberOfKeyUpsRemaining;
let modifierKeyUpsSeenSoFar;

function computeDifference(a, b)
{
    let result = new Set(a);
    for (let item of b)
        result.delete(item);
    return result;
}

// From js-test.js
function areArraysEqual(a, b)
{
    try {
        if (a.length !== b.length)
            return false;
        for (var i = 0; i < a.length; i++)
            if (a[i] !== b[i])
                return false;
    } catch (ex) {
        return false;
    }
    return true;
}

function areKeyCommandsEqual(a, b)
{
    return a.key == b.key && areArraysEqual(a.modifiers, b.modifiers);
}

class KeyCommand {
    constructor(key, modifiers = [])
    {
        this.key = key;
        this.modifiers = modifiers;
    }

    toString()
    {
        return `{ ${this.key}, [${this.modifiers}] }`;
    }
}

function keyCommandsHasCommand(keyCommands, command)
{
    return !!keyCommands.find((k) => areKeyCommandsEqual(k, command));
}

const keys = new Set("abcdefghijklmnopqrstuvwxyz0123456789-=[]\\;',./".split(""));
const deadKeys = new Set("`euin".split(""));
const keysExcludingDeadKeys = computeDifference(keys, deadKeys);
const functionKeys = new Set(["F5", "F6", "F13", "F14", "F15"]);

// FIXME: For some reason the following keys do not emit DOM key events (why?).
const keysToSkip = new Set(["\\"]);
const keysExcludingDeadAndSkippedKeys = computeDifference(keysExcludingDeadKeys, keysToSkip);

let disallowedKeyCommands = [
    new KeyCommand("l", ["metaKey"]),
    new KeyCommand("f", ["metaKey", "altKey"]),
    new KeyCommand("t", ["metaKey"]),
    new KeyCommand("w", ["metaKey"]),
    new KeyCommand("w", ["metaKey", "altKey"]),
    new KeyCommand("n", ["metaKey"]),
    new KeyCommand("\t", ["ctrlKey"]),
    new KeyCommand("\t", ["ctrlKey", "shiftKey"]),
    new KeyCommand("[", ["metaKey"]),
    new KeyCommand("]", ["metaKey"]),
    new KeyCommand("r", ["metaKey"]),
    new KeyCommand("r", ["metaKey", "altKey"]),
];
for (let i = 1; i <= 9; ++i)
    disallowedKeyCommands.push(new KeyCommand(i, ["metaKey"]));

const modifierKeys = ["metaKey", "altKey", "ctrlKey", "shiftKey"];

let tests = [];

function handleKeyDown(event)
{
    logKeyEvent(event);
}

function handleKeyUp(event)
{
    logKeyEvent(event);

    switch (event.key) {
    case "Alt":
         modifierKeyUpsSeenSoFar.unshift("altKey");
         break
    case "Control":
         modifierKeyUpsSeenSoFar.unshift("ctrlKey");
         break  
    case "Meta":
        modifierKeyUpsSeenSoFar.unshift("metaKey");
        break;
    case "Shift":
        modifierKeyUpsSeenSoFar.unshift("shiftKey");
        break;
    case "CapsLock":
        modifierKeyUpsSeenSoFar.unshift("capsLockKey");
        break;
    }

    --numberOfKeyUpsRemaining;

    // Either we received all keyup events or we are missing a keyup for the non-modifier key. The latter can happen
    // if we triggered a key command (e.g. Command + Option + b). Regardless, move on to the next test.
    if (!numberOfKeyUpsRemaining || numberOfKeyUpsRemaining == 1 && areArraysEqual(modifierKeyUpsSeenSoFar, currentTest.modifiers))
        nextKeyPress();
}

function handleKeyPress(event)
{
    event.preventDefault();
    logKeyEvent(event);
}

function log(message)
{
    document.getElementById("console").appendChild(document.createTextNode(message + "\n"));
}

function logKeyEvent(event)
{
    let pieces = [];
    for (let propertyName of ["type", "key", "code", "keyIdentifier", "keyCode", "charCode", "keyCode", "which", "altKey", "ctrlKey", "metaKey", "shiftKey", "location", "keyLocation"])
        pieces.push(`${propertyName}: ${event[propertyName]}`);
    log(pieces.join(", "));
}

const modifierDisplayNameMap = {
    "altKey": "Option",
    "ctrlKey": "Control",
    "metaKey": "Command",
    "shiftKey": "Shift",
    "capsLockKey": "Caps Lock",
}

function displayNameForTest(test)
{
    let displayNamesOfModifiers = [];
    for (const modifier of test.modifiers)
        displayNamesOfModifiers.push(modifierDisplayNameMap[modifier]);
    let result = "";
    if (displayNamesOfModifiers.length)
        result += displayNamesOfModifiers.join(" + ") + " + ";
    result += test.key;
    return result;
}

function nextKeyPress()
{
    if (!tests.length) {
        if (window.testRunner)
            testRunner.notifyDone();
        return;
    }
    currentTest = tests.shift();
    modifierKeyUpsSeenSoFar = [];
    const numberOfNonModifierKeys = 1;
    numberOfKeyUpsRemaining = currentTest.modifiers.length + numberOfNonModifierKeys;
    log(`\nTest ${displayNameForTest(currentTest)}:`);
    if (window.testRunner)
        UIHelper.keyDown(currentTest.key, currentTest.modifiers);
}

function runTest()
{
    if (!window.testRunner)
        return;
    tests = tests.filter((k) => !keyCommandsHasCommand(disallowedKeyCommands, k));
    nextKeyPress();
}

function setUp()
{
    document.body.addEventListener("keydown", handleKeyDown, true);
    document.body.addEventListener("keyup", handleKeyUp, true);
    document.body.addEventListener("keypress", handleKeyPress, true);

    runTest();
}

window.addEventListener("load", setUp, true);
