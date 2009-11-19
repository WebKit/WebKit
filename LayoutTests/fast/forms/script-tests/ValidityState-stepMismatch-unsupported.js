description('Check stepMismatch results for types which does not support for step.');

var input = document.createElement('input');

input.type = 'text';
input.step = '3';
input.value = '2';
shouldBe('input.validity.stepMismatch', 'false');

input.type = 'button';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'checkbox';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'color';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'email';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'hidden';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'image';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'khtml_isindex';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'passwd';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'radio';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'reset';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'search';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'submit';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'tel';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'url';
shouldBe('input.validity.stepMismatch', 'false');
input.type = 'file';
shouldBe('input.validity.stepMismatch', 'false');

var successfullyParsed = true;
