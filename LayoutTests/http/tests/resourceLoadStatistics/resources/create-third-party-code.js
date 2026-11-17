window.resultsFromThirdPartyScript = {
    direct: internals.isThirdPartyScriptOnStack(),
    firstPartyFunction: isThirdPartyScriptOnStackFromFirstPartyFunction(),
};

window.thirdPartyFunction = function() { return internals.isThirdPartyScriptOnStack(); };
window.functionCreatedWithEval = eval("(function() { return internals.isThirdPartyScriptOnStack(); })");
window.functionCreatedWithFunctionConstructor = new Function("return internals.isThirdPartyScriptOnStack();");

window.thirdPartyScriptElement = document.createElement("script");
thirdPartyScriptElement.textContent = "window.resultFromScriptElement = internals.isThirdPartyScriptOnStack();";

document.getElementById("target").setAttribute("onclick", "window.resultFromEventHandlerAttribute = internals.isThirdPartyScriptOnStack();");

setTimeout("window.resultFromTimerString = internals.isThirdPartyScriptOnStack();", 0);
