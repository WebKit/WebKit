function makeRope(str1, str2, str3)
{
    return str1 + str2 + str3;
}
noInline(makeRope);

for (var i = 0; i < 1e6; ++i) {
    makeRope("Hello", "Another", "World");
    makeRope(makeRope("Hello", "Another", "World"), "Another", makeRope("Hello", "Another", "World"));
}
