
function setupObjectHooks(hooks)
{
    // Wrapper for these object should be materialized before setting hooks.
    console.log;
    document.webkitRegister
    HTMLSpanElement.prototype;

    Object.defineProperty(Object.prototype, "prototype", {
        get: function() { return hooks.prototypeGet(); },
        set: function(value) { return hooks.prototypeSet(value); }
    });

    Object.defineProperty(Object.prototype, "constructor", {
        get: function() { return hooks.constructorGet(); },
        set: function(value) { return hooks.constructorSet(value); }
    });

    return hooks;
}

function exerciseDocumentRegister()
{
    var myConstructor = null;
    var myPrototype = Object.create(HTMLElement.prototype);
    try {
        myConstructor = document.webkitRegister("x-do-nothing", { prototype: myPrototype });
    } catch (e) { }

    try {
        if (!myConstructor) {
            debug("Constructor object isn't created.");
            return;
        }

        if (myConstructor.prototype != myPrototype) {
            console.log("FAIL: bad prototype");
            return;
         }

        var element = new myConstructor();
        if (!element)
            return;
        if (element.constructor != myConstructor) {
            console.log("FAIL: bad constructor");
            return;
         }
    } catch (e) { console.log(e); }
}
