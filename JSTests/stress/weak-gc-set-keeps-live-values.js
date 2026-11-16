// A JSGlobalObject caches the JSCustomGetterFunction and JSCustomSetterFunction it builds for a
// custom accessor in a WeakGCSet, so two descriptors for the same property hand back one shared
// function for as long as it stays reachable. Eden and full collections reconcile those sets by
// different paths, so exercise both.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

const heldNames = ["input", "$_", "multiline", "$*", "lastMatch", "$&", "lastParen", "$+", "leftContext", "$`"];
const droppedNames = ["rightContext", "$'", "$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9"];
const setterNames = ["input", "$_", "multiline", "$*"];

const getters = heldNames.map((name) => Object.getOwnPropertyDescriptor(RegExp, name).get);
const setters = setterNames.map((name) => Object.getOwnPropertyDescriptor(RegExp, name).set);

for (let i = 0; i < 32; ++i) {
    // Fill and then drop the caches of throwaway realms, so the sets gain entries that die between
    // collections while this realm's entries stay reachable.
    for (let j = 0; j < 4; ++j) {
        const realm = createGlobalObject();
        for (const name of heldNames)
            Object.getOwnPropertyDescriptor(realm.RegExp, name);
    }

    // Entries this realm gains and drops every iteration: a dead one that outlived its cell would
    // be handed back here as freed memory.
    for (const name of droppedNames)
        Object.getOwnPropertyDescriptor(RegExp, name).get.call(RegExp);

    if (i & 1)
        edenGC();
    else
        gc();

    for (let j = 0; j < heldNames.length; ++j)
        shouldBe(Object.getOwnPropertyDescriptor(RegExp, heldNames[j]).get, getters[j]);
    for (let j = 0; j < setterNames.length; ++j)
        shouldBe(Object.getOwnPropertyDescriptor(RegExp, setterNames[j]).set, setters[j]);
}
