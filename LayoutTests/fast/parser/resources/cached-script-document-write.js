if (window.parent.isTestScriptCached) {
    document.write('<div></div>');
    document.body.appendChild(document.createTextNode(document.querySelector('div') ? 'PASS' : 'FAIL'));
}
