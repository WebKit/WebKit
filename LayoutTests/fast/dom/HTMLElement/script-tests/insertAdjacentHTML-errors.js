description('Test insertAdjacentHTML exceptions to make sure they match HTML5');

var div = document.createElement("div");

shouldBeUndefined("div.insertAdjacentHTML('beforeBegin', 'text')");
shouldBeUndefined("div.insertAdjacentHTML('afterEnd', 'text')");

shouldThrow("div.insertAdjacentHTML('FOO', 'text')", '"SyntaxError (DOM Exception 12): The string did not match the expected pattern."');
shouldThrow("document.documentElement.insertAdjacentHTML('afterEnd', 'text')", '"NoModificationAllowedError (DOM Exception 7): The object can not be modified."');
