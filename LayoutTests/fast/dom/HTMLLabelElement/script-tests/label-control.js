description('Test that label.control references the correct form control, or null if there is no associated control.');

debug("Find a control that is the first descendent in a label");
document.write("<label id='test1'><input id='inputId1'></label>");
shouldBe("document.getElementById('test1').control.id", "'inputId1'");

debug("Find a control based on a label with valid 'for' attribute");
document.write("<label id='test2' for='inputId2'></label><input id='inputId2' type='number'>");
shouldBe("document.getElementById('test2').htmlFor", "'inputId2'");
shouldBe("document.getElementById('test2').control.type", "'number'");

debug("Find a control in p element in label");
document.write("<label id='test3'><p><input id='inputId3' type='date'></p></label>");
shouldBe("document.getElementById('test3').control.id", "'inputId3'");

debug("Find a control in fieldset in label.");
debug("Note that filedset is a form control that is not labelable.");
document.write("<label id='test4'><fieldset><input id='inputId4'></fieldset></label>");
shouldBe("document.getElementById('test4').control.id", "'inputId4'");

debug("Find a control in legend in label.");
debug("Note that legend is a form control that is not labelable.");
document.write("<label id='test5'><legend><input id='inputId5'></legend></label>");
shouldBe("document.getElementById('test5').control.id", "'inputId5'");

debug("Find a control in optgroup in label.");
debug("Note that optgroup is a form control that is not labelable.");
document.write("<label id='test6'><optgroup><input id='inputId6'></optgroup></label>");
shouldBe("document.getElementById('test6').control.id", "'inputId6'");

debug("Find a control in option in label.");
debug("Note that option is a form control that is not labelable.");
document.write("<label id='test7'><option><input id='inputId7'></option></label>");
shouldBe("document.getElementById('test7').control.id", "'inputId7'");

debug("Test label with 'for' attribute which is not a valid element id");
document.write("<label for='foo' id='test8'><input id='inputId8'></label>");
shouldBe("document.getElementById('test8').control", "null");

debug("Test label with 'for' attribute which is not a form control");
document.write("<label for='divId' id='test9'><input id='inputId9'></label><div id='divId'></div>");
shouldBe("document.getElementById('test9').htmlFor", "'divId'");
shouldBe("document.getElementById('test9').control", "null");

debug("Test label with 'for' attribute which is not a labelable form control - fieldset");
document.write("<label for='fsId' id='test10'><input id='inputId10'></label><fieldset id='fsId'></fieldset>");
shouldBe("document.getElementById('test10').htmlFor", "'fsId'");
shouldBe("document.getElementById('test10').control", "null");

debug("Test label with 'for' attribute which is not a labelable form control - legend");
document.write("<label for='legendId' id='test11'><input id='inputId11'></label><legend id='legendId'></legend>");
shouldBe("document.getElementById('test11').htmlFor", "'legendId'");
shouldBe("document.getElementById('test11').control", "null");

debug("Test label with 'for' attribute which is not a labelable form control - optgroup");
document.write("<label for='optgroupId' id='test12'><input id='inputId12'></label><optgroup id='optgroupId'></optgroup>");
shouldBe("document.getElementById('test12').htmlFor", "'optgroupId'");
shouldBe("document.getElementById('test12').control", "null");

debug("Test label with 'for' attribute which is not a labelable form control - option");
document.write("<label for='optionId' id='test13'><input id='inputId13'></label><option id='optionId'></option>");
shouldBe("document.getElementById('test13').htmlFor", "'optionId'");
shouldBe("document.getElementById('test13').control", "null");

var successfullyParsed = true;
