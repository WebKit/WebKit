if (window.testRunner)
    testRunner.dumpAsText();

document.write('<span id="x"></span>');
document.write("This should be a span: " + document.getElementById('x'));
