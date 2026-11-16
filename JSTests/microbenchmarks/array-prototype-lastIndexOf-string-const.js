function test(array, index) {
    return array.lastIndexOf(index);
}
noInline(test);

const array = [
    "dEXt0TxZQQQasd999!@#$%d^&",
    "AAZZ!!@@**CC77zzxx1122d33",
    "HelloWorldHeldloWorlddABC",
    "abcABC123!@#xfyzXYZ!$d%+=",
    "Zyx9!Zyx9!Zyx9!Zyx9!!d???",
    "LoremIpsum1234!@#$LordemI",
    "QQQQQQQQQQqqqqqqqqqq-----",
    "&&&&1111%%%%2222@@@@33f33",
    "TestStringasdVariousChars",
    "RandomASCII~d~~~====?????",
    "^^^^#####^^^f^^+++++.....",
    "AZaz09!?AZaz09!?AZaz09!?%",
    "Foobar##1122Foobar##1122F",
    "9876543210!@d#$%^&*()_+=-",
    "OneTwo3FourFive6Seven8Nin",
    "Zwxy000111Zwxy000111Zwxy0",
    "Spark!!!Spark???Spark%%%S",
    "ShortAndSweet123??!!Short",
    "BenchmarkCaseHere12345!!?",
    "MultiLineString###000xxx@",
    "ABCDEFGHabcdfefgh1234!!!!",
    "111122223333d4444!!!!####",
    "EndlessVariety?!@#$%^^^^^",
    "PqrstUVWX9876pqrstUVWX987",
    "MixItUpWith5omeWe!rdCHARS",
    "FinallyZzYyXx!@#$4321)(*&",
];
for (var i = 0; i < 1e5; ++i)
    test(array, "BenchmarkCaseHere12345!!?");
