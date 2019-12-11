// Test that we properly handle regex's where the nest pattern doesn't need to consume characters to match.

let re = /(.|.+?|\d*)*x/;

let str = "zzzz";
if (re.test(str))
    throw re + ".test(\"" + str + "\") should return false, but returned true.";

s = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
if (re.test(str))
    throw re + ".test(\"" + str + "\") should return false, but returned true.";
