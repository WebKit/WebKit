description("If a script has a for attribute, then it was intended to only be run under certain conditions, often as a result of a certain window event.<br>Since we don't yet support the full for attribute syntax we would run these scripts as we parsed them, often causing unintentional breakage of the site in question.<br>You should *not* see any failure when running this test. If you do, we're not properly running these scripts only when they were intended to be run.");

// A variable indicates the script-for attribute doesn't get executed immediately.
var scriptForExecuted = false;

function ScriptForAttributeExecute() {
    scriptForExecuted = true;
}

document.write('<script for=window event=onresize> ScriptForAttributeExecute(); </script>');
shouldBe('scriptForExecuted', "false");
document.write('<script for=window event=onresize type="text/javascript"> ScriptForAttributeExecute(); </script>');
shouldBe('scriptForExecuted', "false");
document.write('<script for=window event=onresize language="javascript"> ScriptForAttributeExecute(); </script>');
shouldBe('scriptForExecuted', "false");

var successfullyParsed = true;
