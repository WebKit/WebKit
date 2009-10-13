description("This test aims to check for typeMismatch flag with type=email input fields.");

var i = document.createElement('input');
i.type = 'email';

function emailCheck(val, expectedValid)
{
    i.value = val;
    var vs = i.validity.typeMismatch;
    var didPass = vs == expectedValid;
    var didPassText = didPass ? "a correct" : "an incorrect";
    var validityText = vs ? "invalid" : "valid";
    var multipleText = i.multiple ? "list" : "";
    var resultText = val + " is " + didPassText + " " + validityText + " email address " + multipleText;
    if (didPass)
        testPassed(resultText);
    else
        testFailed(resultText);
}

// VALID
emailCheck("something@something.com", false);
emailCheck("someone@localhost.localdomain", false);
emailCheck("someone@127.0.0.1", false);
emailCheck("a@b.b", false);
emailCheck("a/b@domain.com", false);
emailCheck("{}@domain.com", false);
emailCheck("ddjk-s-jk@asl-.com", false);
emailCheck("m*'!%@something.sa", false);
emailCheck("tu!!7n7.ad##0!!!@company.ca", false);
emailCheck("%@com.com", false);
emailCheck("!#$%&'*+/=?^_`{|}~.-@com.com", false);
emailCheck(".wooly@example.com", false);
emailCheck("wo..oly@example.com", false);
emailCheck("someone@do-ma-in.com", false);
emailCheck("someone@do-.com", false);
emailCheck("somebody@-.com", false);

// INVALID
emailCheck("invalid:email@example.com", true);
emailCheck("@somewhere.com", true);
emailCheck("example.com", true);
emailCheck("@@example.com", true);
emailCheck("a space@example.com", true);
emailCheck("something@ex..ample.com", true);
emailCheck("a\b@c", true);
emailCheck("someone@somewhere.com.", true);
emailCheck("\"\"test\blah\"\"@example.com", true);
emailCheck("\"testblah\"@example.com", true);
emailCheck("someone@somewhere.com@", true);
emailCheck("someone@somewhere_com", true);
emailCheck("someone@some:where.com", true);
emailCheck(".", true);
emailCheck("F/s/f/a@feo+re.com", true);
emailCheck("some+long+email+address@some+host-weird-/looking.com", true);

i.multiple = true;

// VALID MULTIPLE
emailCheck("someone@somewhere.com,john@doe.com,a@b.c,a/b@c.c,ualla@ualla.127", false)
emailCheck("tu!!7n7.ad##0!!!@company.ca,F/s/f/a@feo-re.com,m*'@a.b", false)
emailCheck(",,,,,,,,,,,", false)

// INVALID MULTIPLE (true on the first invalid occurrence)
emailCheck("someone@somewhere.com,john@doe..com,a@b,a/b@c,ualla@ualla.127", true)
emailCheck("some+long+email+address@some+host:weird-/looking.com,F/s/f/a@feo+re.com,,m*'@'!%", true)
emailCheck(",,,,,,,,, ,,", true)

var successfullyParsed = true;
