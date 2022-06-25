/*
Copyright (c) 2019 The Khronos Group Inc.
Use of this source code is governed by an MIT-style license that can be
found in the LICENSE.txt file.
*/

shouldBeTrue("successfullyParsed");
_addSpan('<br /><span class="pass">TEST COMPLETE</span>');
if (_jsTestPreVerboseLogging) {
    _bufferedLogToConsole('TEST COMPLETE');
}

{
    const e_results = document.createElement('div');
    let fails_class = 'pass';
    if (RESULTS.fail) {
        fails_class = 'fail';
    }
    e_results.classList.add('pass');
    e_results.innerHTML = `<p>TEST COMPLETE: ${RESULTS.pass} PASS, ` +
      `<span class="${fails_class}">${RESULTS.fail} FAIL</span></p>`;

    const e_desc = document.getElementById("description");
    e_desc.appendChild(e_results);
}

notifyFinishedToHarness()
