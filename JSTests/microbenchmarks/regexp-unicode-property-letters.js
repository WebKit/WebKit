let phrases = [
    "正規表現は文字列の集合を一つの文字列で表現する方法の一つである",
    "Регулярныевыраженияформальныйязыкпоиска",
    "κανονικήέκφρασηείναιμιαακολουθίαχαρακτήρων",
    "정규표현식은특정한규칙을가진문자열의집합을표현하는데사용하는형식언어이다",
    "aregularexpressionisasequenceofcharactersthatspecifiesamatchpattern",
    "التعبيراتالنمطيةهيسلسلةمنالمحارف",
    "𠮷野家𠀋𠂉𠃌𠄀𠅁𠆢𠇇𠈓𠉁𠊛𠋆𠌫𠍱𠎁𠏹",
];
let text = "";
for (let i = 0; i < 60; ++i)
    text += phrases[(i * 3) % phrases.length] + (i % 4 ? " " : "42, ");

let letters = /\p{L}+/gu;
let identifier = /[\p{L}\p{N}_]+/gu;
let result = 0;
for (let i = 0; i < 20000; ++i) {
    letters.lastIndex = 0;
    while (letters.test(text))
        result++;
    identifier.lastIndex = 0;
    while (identifier.test(text))
        result++;
}
if (result !== 120 * 20000)
    throw new Error("Bad result: " + result);
