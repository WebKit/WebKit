if (window.testRunner) testRunner.waitUntilDone();

// Imports code into a worklet, with some helpers.
async function importWorklet(worklet, code) {
  const response = await fetch("resources/testharness-worklet.js");
  const finalCode = (await response.text()) + code;

  let url;
  // FIXME: This is temporary, until we support loading scripts by url
  if (window.chrome) {
    const blob = new Blob([finalCode], {type: 'text/javascript'});
    url = URL.createObjectURL(blob);
  } else {
    url = finalCode;
  }

  worklet.addModule(url);
  if (window.testRunner) {
    setTimeout(function() {
      testRunner.notifyDone();
    }, 3000);
  }
}
