description("Tests that scripts which have a for-event other than window.onload are not executed.");

document.write('<script for="window">testPassed(\'for=window\');</script>');
document.write('<script for="anything">testPassed(\'for=anything\');</script>');
document.write('<script event="onload">testPassed(\'event=onload\');</script>');
document.write('<script event="anything">testPassed(\'event=anything\');</script>');
document.write('<script for="window" event="onload">testPassed(\'for=window event=onload\');</script>');
document.write('<script for="window" event="onload()">testPassed(\'for=window event=onload()\');</script>');
document.write('<script for="  WINDOW  " event="  ONLOAD()  ">testPassed(\'for=WINDOW event=ONLOAD()\');</script>');
document.write('<script for="window" event="onresize">testFailed(\'for=window event=onresize\');</script>');
document.write('<script for="document" event="onload">testFailed(\'for=document event=onload\');</script>');
document.write('<script for="document" event="onclick">testFailed(\'for=document event=onclick\');</script>');

var successfullyParsed = true;
