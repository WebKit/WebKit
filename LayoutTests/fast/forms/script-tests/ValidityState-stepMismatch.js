description('Check stepMismatch results for unsupported types.');

var input = document.createElement('input');
document.body.appendChild(input);

debug('Unsupported types');
shouldBe('input.type = "text"; input.step = "3"; input.min = ""; input.value = "2"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "button"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "checkbox"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "color"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "email"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "hidden"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "image"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "khtml_isindex"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "passwd"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "radio"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "reset"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "search"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "submit"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "tel"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "url"; input.validity.stepMismatch', 'false');
shouldBe('input.type = "file"; input.validity.stepMismatch', 'false');
