description(
"This test checks Unicode in negative RegExp character classes."
);

function test(pattern, str, expected_length) {
  var result = eval('"' + str + '"').replace(new RegExp(pattern, 'img'), '');
  
  if (result.length == expected_length)
    testPassed('"' + pattern + '", ' + '"' + str + '".');
  else
    testFailed('"' + pattern + '", ' + '"' + str + '". Was "' + result + '".');
}


test("\\s", " \\t\\f\\v\\r\\n", 0); // ASCII whitespace.
test("\\S", "Проверка", 0); // Cyrillic letters are non-whitespace...
test("\\s", "Проверка", 8); // ...and they aren't whitespace.
test("[\\s]", "Проверка", 8);
test("[\\S]", "Проверка", 0);
test("[^\\s]", "Проверка", 0);
test("[^\\S]", "Проверка", 8);
test("[\\s\\S]*", "\\u2002Проверка\\r\\n\\u00a0", 0);
test("\\S\\S", "уф", 0);
test("\\S{2}", "уф", 0);

test("\\w", "Проверка", 8); // Alas, only ASCII characters count as word ones in JS.
test("\\W", "Проверка", 0);
test("[\\w]", "Проверка", 8);
test("[\\W]", "Проверка", 0);
test("[^\\w]", "Проверка", 0);
test("[^\\W]", "Проверка", 8);
test("\\W\\W", "уф", 0);
test("\\W{2}", "уф", 0);

test("\\d", "Проверка", 8); // Digit and non-digit.
test("\\D", "Проверка", 0);
test("[\\d]", "Проверка", 8);
test("[\\D]", "Проверка", 0);
test("[^\\d]", "Проверка", 0);
test("[^\\D]", "Проверка", 8);
test("\\D\\D", "уф", 0);
test("\\D{2}", "уф", 0);

test("[\\S\\d]", "Проверка123", 0);
test("[\\d\\S]", "Проверка123", 0);
test("[^\\S\\d]", "Проверка123", 11);
test("[^\\d\\S]", "Проверка123", 11);

test("[ \\S]", " Проверка ", 0);
test("[\\S ]", " Проверка ", 0);
test("[ф \\S]", " Проверка ", 0);
test("[\\Sф ]", " Проверка ", 0);

test("[^р\\S]", " Проверка ", 8);
test("[^\\Sр]", " Проверка ", 8);
test("[^р\\s]", " Проверка ", 4);
test("[^\\sр]", " Проверка ", 4);

test("[ф \\s\\S]", "Проверка \\r\\n", 0);
test("[\\S\\sф ]", "Проверка \\r\\n", 0);

test("[^z]", "Проверка \\r\\n", 0);
test("[^ф]", "Проверка \\r\\n", 0);

var successfullyParsed = true;
