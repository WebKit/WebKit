description('Test insertAdjacentHTML exceptions to make sure they match HTML5');

var div = document.createElement("div");

shouldBeUndefined("div.insertAdjacentHTML('beforeBegin', 'text')");
shouldBeUndefined("div.insertAdjacentHTML('afterEnd', 'text')");

shouldThrow("div.insertAdjacentHTML('FOO', 'text')", '"Error: SYNTAX_ERR: DOM Exception 12"');
shouldThrow("document.documentElement.insertAdjacentHTML('afterEnd', 'text')", '"Error: NO_MODIFICATION_ALLOWED_ERR: DOM Exception 7"');
