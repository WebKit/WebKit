description("JSRegress/" + ("" + window.location).split('/').pop().split('.')[0]);
_JSRegress_didSucceed = true;
_JSRegress_oldOnError = window.onerror;
window.onerror = function(message) {
    debug("FAIL caught exception: " + message);
    _JSRegress_didSucceed = false;
    _JSRegress_oldOnError.apply(this, arguments);
}

if (typeof noInline == "undefined") {
    if (window.testRunner)
        noInline =window.testRunner.neverInlineFunction || function(){};
    else
        noInline = function(){}
}

