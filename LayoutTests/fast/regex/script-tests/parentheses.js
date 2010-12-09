description("This page tests handling of parentheses subexpressions.");

var regexp1 = /(a|A)(b|B)/;
shouldBe("regexp1.exec('abc')", "['ab','a','b']");

var regexp2 = /(a((b)|c|d))e/;
shouldBe("regexp2.exec('abacadabe')", "['abe','ab','b','b']");

var regexp3 = /(a(b|(c)|d))e/;
shouldBe("regexp3.exec('abacadabe')", "['abe','ab','b',undefined]");

var regexp4 = /(a(b|c|(d)))e/;
shouldBe("regexp4.exec('abacadabe')", "['abe','ab','b',undefined]");

var regexp5 = /(a((b)|(c)|(d)))e/;
shouldBe("regexp5.exec('abacadabe')", "['abe','ab','b','b',undefined,undefined]");

var regexp6 = /(a((b)|(c)|(d)))/;
shouldBe("regexp6.exec('abcde')", "['ab','ab','b','b',undefined,undefined]");

var regexp7 = /(a(b)??)??c/;
shouldBe("regexp7.exec('abc')", "['abc','ab','b']");

var regexp8 = /(a|(e|q))(x|y)/;
shouldBe("regexp8.exec('bcaddxqy')" , "['qy','q','q','y']");

var regexp9 = /((t|b)?|a)$/;
shouldBe("regexp9.exec('asdfjejgsdflaksdfjkeljghkjea')", "['a','a',undefined]");

var regexp10 = /(?:h|e?(?:t|b)?|a?(?:t|b)?)(?:$)/;
shouldBe("regexp10.exec('asdfjejgsdflaksdfjkeljghat')", "['at']");

var regexp11 = /([Jj]ava([Ss]cript)?)\sis\s(fun\w*)/;
shouldBeNull("regexp11.exec('Developing with JavaScript is dangerous, do not try it without assistance')");

var regexp12 = /(?:(.+), )?(.+), (..) to (?:(.+), )?(.+), (..)/;
shouldBe("regexp12.exec('Seattle, WA to Buckley, WA')", "['Seattle, WA to Buckley, WA', undefined, 'Seattle', 'WA', undefined, 'Buckley', 'WA']");

var regexp13 = /(A)?(A.*)/;
shouldBe("regexp13.exec('zxcasd;fl\ ^AaaAAaaaf;lrlrzs')", "['AaaAAaaaf;lrlrzs',undefined,'AaaAAaaaf;lrlrzs']");

var regexp14 = /(a)|(b)/;
shouldBe("regexp14.exec('b')", "['b',undefined,'b']");

var regexp15 = /^(?!(ab)de|x)(abd)(f)/;
shouldBe("regexp15.exec('abdf')", "['abdf',undefined,'abd','f']");

var regexp16 = /(a|A)(b|B)/;
shouldBe("regexp16.exec('abc')", "['ab','a','b']");

var regexp17 = /(a|d|q|)x/i;
shouldBe("regexp17.exec('bcaDxqy')", "['Dx','D']");

var regexp18 = /^.*?(:|$)/;
shouldBe("regexp18.exec('Hello: World')", "['Hello:',':']");

var regexp19 = /(ab|^.{0,2})bar/;
shouldBe("regexp19.exec('barrel')", "['bar','']");

var regexp20 = /(?:(?!foo)...|^.{0,2})bar(.*)/;
shouldBe("regexp20.exec('barrel')", "['barrel','rel']");
shouldBe("regexp20.exec('2barrel')", "['2barrel','rel']");

var regexp21 = /([a-g](b|B)|xyz)/;
shouldBe("regexp21.exec('abc')", "['ab','ab','b']");

var regexp22 = /(?:^|;)\s*abc=([^;]*)/;
shouldBeNull("regexp22.exec('abcdlskfgjdslkfg')");

var regexp23 = /"[^<"]*"|'[^<']*'/;
shouldBe("regexp23.exec('<html xmlns=\"http://www.w3.org/1999/xhtml\"')", "['\"http://www.w3.org/1999/xhtml\"']");

var regexp24 = /^(?:(?=abc)\w{3}:|\d\d)$/;
shouldBeNull("regexp24.exec('123')");

var regexp25 = /^\s*(\*|[\w\-]+)(\b|$)?/;
shouldBe("regexp25.exec('this is a test')", "['this','this',undefined]");
shouldBeNull("regexp25.exec('!this is a test')");

var regexp26 = /a(b)(a*)|aaa/;
shouldBe("regexp26.exec('aaa')", "['aaa',undefined,undefined]");

var regexp27 = new RegExp(
    "^" +
    "(?:" +
        "([^:/?#]+):" + /* scheme */
    ")?" +
    "(?:" +
        "(//)" + /* authorityRoot */
        "(" + /* authority */
            "(?:" +
                "(" + /* userInfo */
                    "([^:@]*)" + /* user */
                    ":?" +
                    "([^:@]*)" + /* password */
                ")?" +
                "@" +
            ")?" +
            "([^:/?#]*)" + /* domain */
            "(?::(\\d*))?" + /* port */
        ")" +
    ")?" +
    "([^?#]*)" + /*path*/
    "(?:\\?([^#]*))?" + /* queryString */
    "(?:#(.*))?" /*fragment */
);
shouldBe("regexp27.exec('file:///Users/Someone/Desktop/HelloWorld/index.html')", "['file:///Users/Someone/Desktop/HelloWorld/index.html','file','//','',undefined,undefined,undefined,'',undefined,'/Users/Someone/Desktop/HelloWorld/index.html',undefined,undefined]");

var regexp28 = new RegExp(
    "^" +
    "(?:" +
        "([^:/?#]+):" + /* scheme */
    ")?" +
    "(?:" +
        "(//)" + /* authorityRoot */
        "(" + /* authority */
            "(" + /* userInfo */
                "([^:@]*)" + /* user */
                ":?" +
                "([^:@]*)" + /* password */
            ")?" +
            "@" +
        ")" +
    ")?"
);
shouldBe("regexp28.exec('file:///Users/Someone/Desktop/HelloWorld/index.html')", "['file:','file',undefined,undefined,undefined,undefined,undefined]");

shouldBe("'Hi Bob'.match(/(Rob)|(Bob)|(Robert)|(Bobby)/)", "['Bob',undefined,'Bob',undefined,undefined]");

var successfullyParsed = true;

