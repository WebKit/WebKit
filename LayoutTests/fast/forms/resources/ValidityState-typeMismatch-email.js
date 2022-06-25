description("This test aims to check for typeMismatch flag and sanitization with type=email input fields.");

var i = document.createElement('input');
i.type = 'email';

function emailCheck(value, expectedValue, expectedMismatch, multiple)
{
    i.value = value;
    i.multiple = !!multiple;
    var mismatch = i.validity.typeMismatch;
    var mismatchPass = mismatch == expectedMismatch;
    var sanitizePass = i.value == expectedValue;
    var mismatchResult = '"' + value + '" is a ' + (mismatch ? 'invalid' : 'valid') + ' email address' + (multiple ? ' list. ' : '. ');
    var sanitizeResult = 'It was sanitized to "' + i.value + '"' + (sanitizePass ? '.' : ', but should be sanitized to "' + expectedValue + '"');
    var result = mismatchResult;
    if (value != expectedValue || !sanitizePass)
        result += sanitizeResult;

    if (mismatchPass && sanitizePass)
        testPassed(result);
    else
        testFailed(result);
}

var expectValid = false;
var expectInvalid = true;
var multiple = true;

debug("Valid single addresses when 'multiple' attribute is not set.");
emailCheck("something@something.com", "something@something.com", expectValid);
emailCheck("someone@localhost.localdomain", "someone@localhost.localdomain", expectValid);
emailCheck("someone@127.0.0.1", "someone@127.0.0.1", expectValid);
emailCheck("a@b.b", "a@b.b", expectValid);
emailCheck("a/b@domain.com", "a/b@domain.com", expectValid);
emailCheck("{}@domain.com", "{}@domain.com", expectValid);
emailCheck("m*'!%@something.sa", "m*'!%@something.sa", expectValid);
emailCheck("tu!!7n7.ad##0!!!@company.ca", "tu!!7n7.ad##0!!!@company.ca", expectValid);
emailCheck("%@com.com", "%@com.com", expectValid);
emailCheck("!#$%&'*+/=?^_`{|}~.-@com.com", "!#$%&'*+/=?^_`{|}~.-@com.com", expectValid);
emailCheck(".wooly@example.com", ".wooly@example.com", expectValid);
emailCheck("wo..oly@example.com", "wo..oly@example.com", expectValid);
emailCheck("someone@do-ma-in.com", "someone@do-ma-in.com", expectValid);
emailCheck("somebody@example", "somebody@example", expectValid);
emailCheck("\u000Aa@p.com\u000A", "a@p.com", expectValid);
emailCheck("\u000Da@p.com\u000D", "a@p.com", expectValid);
emailCheck("a\u000A@p.com", "a@p.com", expectValid);
emailCheck("a\u000D@p.com", "a@p.com", expectValid);
emailCheck("", "", expectValid);
emailCheck(" ", "", expectValid);
emailCheck(" a@p.com", "a@p.com", expectValid);
emailCheck("a@p.com ", "a@p.com", expectValid);
emailCheck(" a@p.com ", "a@p.com", expectValid);
emailCheck("\u0020a@p.com\u0020", "a@p.com", expectValid);
emailCheck("\u0009a@p.com\u0009", "a@p.com", expectValid);
emailCheck("\u000Ca@p.com\u000C", "a@p.com", expectValid);
emailCheck("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@p.com", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@p.com", expectValid); // 64 characters in left part.
emailCheck("a@ppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp.ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", "a@ppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp.ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", expectValid); // Labels with 63 characters.

debug("Invalid single addresses when 'multiple' attribute is not set.");
emailCheck("invalid:email@example.com", "invalid:email@example.com", expectInvalid);
emailCheck("@somewhere.com", "@somewhere.com", expectInvalid);
emailCheck("example.com", "example.com", expectInvalid);
emailCheck("@@example.com", "@@example.com", expectInvalid);
emailCheck("a space@example.com", "a space@example.com", expectInvalid);
emailCheck("something@ex..ample.com", "something@ex..ample.com", expectInvalid);
emailCheck("a\b@c", "a\b@c", expectInvalid);
emailCheck("someone@somewhere.com.", "someone@somewhere.com.", expectInvalid);
emailCheck("\"\"test\blah\"\"@example.com", "\"\"test\blah\"\"@example.com", expectInvalid);
emailCheck("\"testblah\"@example.com", "\"testblah\"@example.com", expectInvalid);
emailCheck("someone@somewhere.com@", "someone@somewhere.com@", expectInvalid);
emailCheck("someone@somewhere_com", "someone@somewhere_com", expectInvalid);
emailCheck("someone@some:where.com", "someone@some:where.com", expectInvalid);
emailCheck(".", ".", expectInvalid);
emailCheck("F/s/f/a@feo+re.com", "F/s/f/a@feo+re.com", expectInvalid);
emailCheck("some+long+email+address@some+host-weird-/looking.com", "some+long+email+address@some+host-weird-/looking.com", expectInvalid);
emailCheck("a @p.com", "a @p.com", expectInvalid);
emailCheck("a\u0020@p.com", "a\u0020@p.com", expectInvalid);
emailCheck("a\u0009@p.com", "a\u0009@p.com", expectInvalid);
emailCheck("a\u000B@p.com", "a\u000B@p.com", expectInvalid);
emailCheck("a\u000C@p.com", "a\u000C@p.com", expectInvalid);
emailCheck("a\u2003@p.com", "a\u2003@p.com", expectInvalid);
emailCheck("a\u3000@p.com", "a\u3000@p.com", expectInvalid);
emailCheck("ddjk-s-jk@asl-.com", "ddjk-s-jk@asl-.com", expectInvalid); // Domain should end with a letter or a digit.
emailCheck("someone@do-.com", "someone@do-.com", expectInvalid); // Domain should end with a letter or a digit.
emailCheck("somebody@-.com", "somebody@-.com", expectInvalid); // Domain should start with a letter or a digit.
emailCheck("a@pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp.com", "a@pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp.com", expectInvalid); // Label with 64 characters.
emailCheck("a@p.cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", "a@p.cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", expectInvalid); // Label with 64 characters.

debug("Valid single addresses when 'multiple' attribute is set.");
emailCheck("something@something.com", "something@something.com", expectValid, multiple);
emailCheck("someone@localhost.localdomain", "someone@localhost.localdomain", expectValid, multiple);
emailCheck("someone@127.0.0.1", "someone@127.0.0.1", expectValid, multiple);
emailCheck("a@b.b", "a@b.b", expectValid, multiple);
emailCheck("a/b@domain.com", "a/b@domain.com", expectValid, multiple);
emailCheck("{}@domain.com", "{}@domain.com", expectValid, multiple);
emailCheck("m*'!%@something.sa", "m*'!%@something.sa", expectValid, multiple);
emailCheck("tu!!7n7.ad##0!!!@company.ca", "tu!!7n7.ad##0!!!@company.ca", expectValid, multiple);
emailCheck("%@com.com", "%@com.com", expectValid, multiple);
emailCheck("!#$%&'*+/=?^_`{|}~.-@com.com", "!#$%&'*+/=?^_`{|}~.-@com.com", expectValid, multiple);
emailCheck(".wooly@example.com", ".wooly@example.com", expectValid, multiple);
emailCheck("wo..oly@example.com", "wo..oly@example.com", expectValid, multiple);
emailCheck("someone@do-ma-in.com", "someone@do-ma-in.com", expectValid, multiple);
emailCheck("somebody@example", "somebody@example", expectValid, multiple);
emailCheck("\u0020a@p.com\u0020", "a@p.com", expectValid, multiple);
emailCheck("\u0009a@p.com\u0009", "a@p.com", expectValid, multiple);
emailCheck("\u000Aa@p.com\u000A", "a@p.com", expectValid, multiple);
emailCheck("\u000Ca@p.com\u000C", "a@p.com", expectValid, multiple);
emailCheck("\u000Da@p.com\u000D", "a@p.com", expectValid, multiple);
emailCheck("a\u000A@p.com", "a@p.com", expectValid, multiple);
emailCheck("a\u000D@p.com", "a@p.com", expectValid, multiple);
emailCheck("", "", expectValid, multiple);
emailCheck(" ", "", expectValid, multiple);
emailCheck(" a@p.com", "a@p.com", expectValid, multiple);
emailCheck("a@p.com ", "a@p.com", expectValid, multiple);
emailCheck(" a@p.com ", "a@p.com", expectValid, multiple);
emailCheck("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@p.com", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@p.com", expectValid, multiple); // 64 characters in left part.
emailCheck("a@ppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp.ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", "a@ppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp.ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", expectValid, multiple); // Labels with 63 characters.

debug("Invalid single addresses when 'multiple' attribute is set.");
emailCheck("invalid:email@example.com", "invalid:email@example.com", expectInvalid, multiple);
emailCheck("@somewhere.com", "@somewhere.com", expectInvalid, multiple);
emailCheck("example.com", "example.com", expectInvalid, multiple);
emailCheck("@@example.com", "@@example.com", expectInvalid, multiple);
emailCheck("a space@example.com", "a space@example.com", expectInvalid, multiple);
emailCheck("something@ex..ample.com", "something@ex..ample.com", expectInvalid, multiple);
emailCheck("a\b@c", "a\b@c", expectInvalid, multiple);
emailCheck("someone@somewhere.com.", "someone@somewhere.com.", expectInvalid, multiple);
emailCheck("\"\"test\blah\"\"@example.com", "\"\"test\blah\"\"@example.com", expectInvalid, multiple);
emailCheck("\"testblah\"@example.com", "\"testblah\"@example.com", expectInvalid, multiple);
emailCheck("someone@somewhere.com@", "someone@somewhere.com@", expectInvalid, multiple);
emailCheck("someone@somewhere_com", "someone@somewhere_com", expectInvalid, multiple);
emailCheck("someone@some:where.com", "someone@some:where.com", expectInvalid, multiple);
emailCheck(".", ".", expectInvalid, multiple);
emailCheck("F/s/f/a@feo+re.com", "F/s/f/a@feo+re.com", expectInvalid, multiple);
emailCheck("some+long+email+address@some+host-weird-/looking.com", "some+long+email+address@some+host-weird-/looking.com", expectInvalid, multiple);
emailCheck("\u000Ba@p.com\u000B", "\u000Ba@p.com\u000B", expectInvalid, multiple);
emailCheck("\u2003a@p.com\u2003", "\u2003a@p.com\u2003", expectInvalid, multiple);
emailCheck("\u3000a@p.com\u3000", "\u3000a@p.com\u3000", expectInvalid, multiple);
emailCheck("a @p.com", "a @p.com", expectInvalid, multiple);
emailCheck("a\u0020@p.com", "a\u0020@p.com", expectInvalid, multiple);
emailCheck("a\u0009@p.com", "a\u0009@p.com", expectInvalid, multiple);
emailCheck("a\u000B@p.com", "a\u000B@p.com", expectInvalid, multiple);
emailCheck("a\u000C@p.com", "a\u000C@p.com", expectInvalid, multiple);
emailCheck("a\u2003@p.com", "a\u2003@p.com", expectInvalid, multiple);
emailCheck("a\u3000@p.com", "a\u3000@p.com", expectInvalid, multiple);
emailCheck("ddjk-s-jk@asl-.com", "ddjk-s-jk@asl-.com", expectInvalid, multiple); // Domain should end with a letter or a digit.
emailCheck("someone@do-.com", "someone@do-.com", expectInvalid, multiple); // Domain should end with a letter or a digit.
emailCheck("somebody@-.com", "somebody@-.com", expectInvalid, multiple); // Domain should start with a letter or a digit.
emailCheck("a@pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp.com", "a@pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp.com", expectInvalid, multiple); // Label with 64 characters.
emailCheck("a@p.cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", "a@p.cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", expectInvalid, multiple); // Label with 64 characters.

debug("Valid multiple addresses when 'multiple' attribute is set.");
emailCheck("someone@somewhere.com,john@doe.com,a@b.c,a/b@c.c,ualla@ualla.127", "someone@somewhere.com,john@doe.com,a@b.c,a/b@c.c,ualla@ualla.127", expectValid, multiple);
emailCheck("tu!!7n7.ad##0!!!@company.ca,F/s/f/a@feo-re.com,m*'@a.b", "tu!!7n7.ad##0!!!@company.ca,F/s/f/a@feo-re.com,m*'@a.b", expectValid, multiple);
emailCheck(" a@p.com,b@p.com", "a@p.com,b@p.com", expectValid, multiple);
emailCheck("a@p.com ,b@p.com", "a@p.com,b@p.com", expectValid, multiple);
emailCheck("a@p.com, b@p.com", "a@p.com,b@p.com", expectValid, multiple);
emailCheck("a@p.com,b@p.com ", "a@p.com,b@p.com", expectValid, multiple);
emailCheck("   a@p.com   ,   b@p.com   ", "a@p.com,b@p.com", expectValid, multiple);
emailCheck("\u0020a@p.com\u0020,\u0020b@p.com\u0020", "a@p.com,b@p.com", expectValid, multiple);
emailCheck("\u0009a@p.com\u0009,\u0009b@p.com\u0009", "a@p.com,b@p.com", expectValid, multiple);
emailCheck("\u000Aa@p.com\u000A,\u000Ab@p.com\u000A", "a@p.com,b@p.com", expectValid, multiple);
emailCheck("\u000Ca@p.com\u000C,\u000Cb@p.com\u000C", "a@p.com,b@p.com", expectValid, multiple);
emailCheck("\u000Da@p.com\u000D,\u000Db@p.com\u000D", "a@p.com,b@p.com", expectValid, multiple);

debug("Invalid multiple addresses when 'multiple' attribute is set.");
emailCheck("someone@somewhere.com,john@doe..com,a@b,a/b@c,ualla@ualla.127", "someone@somewhere.com,john@doe..com,a@b,a/b@c,ualla@ualla.127", expectInvalid, multiple);
emailCheck("some+long+email+address@some+host:weird-/looking.com,F/s/f/a@feo+re.com,,m*'@'!%", "some+long+email+address@some+host:weird-/looking.com,F/s/f/a@feo+re.com,,m*'@'!%", expectInvalid, multiple);
emailCheck("   a @p.com   ,   b@p.com   ", "a @p.com,b@p.com", expectInvalid, multiple);
emailCheck("   a@p.com   ,   b @p.com   ", "a@p.com,b @p.com", expectInvalid, multiple);
emailCheck("\u000Ba@p.com\u000B,\u000Bb@p.com\u000B", "\u000Ba@p.com\u000B,\u000Bb@p.com\u000B", expectInvalid, multiple);
emailCheck("\u2003a@p.com\u2003,\u2003b@p.com\u2003", "\u2003a@p.com\u2003,\u2003b@p.com\u2003", expectInvalid, multiple);
emailCheck("\u3000a@p.com\u3000,\u3000b@p.com\u3000", "\u3000a@p.com\u3000,\u3000b@p.com\u3000", expectInvalid, multiple);
emailCheck(",,", ",,", expectInvalid, multiple);
emailCheck(" ,,", ",,", expectInvalid, multiple);
emailCheck(", ,", ",,", expectInvalid, multiple);
emailCheck(",, ", ",,", expectInvalid, multiple);
emailCheck("  ,  ,  ", ",,", expectInvalid, multiple);
emailCheck("\u0020,\u0020,\u0020", ",,", expectInvalid, multiple);
emailCheck("\u0009,\u0009,\u0009", ",,", expectInvalid, multiple);
emailCheck("\u000A,\u000A,\u000A", ",,", expectInvalid, multiple);
emailCheck("\u000B,\u000B,\u000B", "\u000B,\u000B,\u000B", expectInvalid, multiple);
emailCheck("\u000C,\u000C,\u000C", ",,", expectInvalid, multiple);
emailCheck("\u000D,\u000D,\u000D", ",,", expectInvalid, multiple);
emailCheck("\u2003,\u2003,\u2003", "\u2003,\u2003,\u2003", expectInvalid, multiple);
emailCheck("\u3000,\u3000,\u3000", "\u3000,\u3000,\u3000", expectInvalid, multiple);
