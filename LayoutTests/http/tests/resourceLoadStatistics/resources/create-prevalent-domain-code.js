window.resultsFromPrevalentDomainScript = {
    direct: internals.isPrevalentDomainScriptOnStack(),
    firstPartyFunction: isPrevalentDomainScriptOnStackFromFirstPartyFunction(),
};

window.prevalentDomainFunction = function() { return internals.isPrevalentDomainScriptOnStack(); };
window.functionCreatedWithEval = eval("(function() { return internals.isPrevalentDomainScriptOnStack(); })");
window.functionCreatedWithFunctionConstructor = new Function("return internals.isPrevalentDomainScriptOnStack();");

window.prevalentDomainScriptElement = document.createElement("script");
prevalentDomainScriptElement.textContent = "window.resultFromScriptElement = internals.isPrevalentDomainScriptOnStack();";

document.getElementById("target").setAttribute("onclick", "window.resultFromEventHandlerAttribute = internals.isPrevalentDomainScriptOnStack();");

setTimeout("window.resultFromTimerString = internals.isPrevalentDomainScriptOnStack();", 0);
