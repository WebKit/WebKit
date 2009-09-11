description("Input type=url validation test");

function check(value, mismatchExpected) {
    i.value = value;
    var actual = i.validity.typeMismatch;
    var didPass = actual == mismatchExpected;
    var resultText = value + ' is ' + (didPass ? 'a correct ' : 'an incorrect ') + (actual ? 'invalid' : 'valid') + ' url.';
    if (didPass)
        testPassed(resultText);
    else
        testFailed(resultText);
}

var i = document.createElement('input');
i.type = 'url';

// Valid values
check('http://www.google.com', false);
check('http://localhost', false);
check('http://127.0.0.1', false);
check('http://a', false);
check('http://www.google.com/search?rls=en&q=WebKit&ie=UTF-8&oe=UTF-8', false);
check('ftp://ftp.myhost.com', false);
check('ssh://ssh.myhost.com', false);
check('somescheme://ssh.myhost.com', false);
check('http://a/\\\/\'\'*<>/', false);
check('http://a/dfs/\kds@sds', false);
check('http://a.a:1/search?a&b', false);

// Invalid values
check('www.google.com', true);
check('127.0.0.1', true);
check('.com', true);
check('http://www.g**gle.com', true);
check('http://www.google.com:aaaa', true);
check('http:// www.google.com', true);
check('http://www .google.com', true);
check('http://www.&#10;google.&#13;com', true);
check('://', true);
check('/http://www.google.com', true);
check('----ftp://a', true);
check('scheme//a', true);
check('http://host+', true);
check('http://myurl!', true);

var successfullyParsed = true;
