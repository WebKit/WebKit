// Regression test for rdar://173140757
// YARR JIT null ParenContext crash in Greedy/NonGreedy backtrack path
// of ParenthesesSubpatternBegin. A NonGreedy (*?) group nested inside
// a FixedCount ({3}) group with multiple alternatives can backtrack
// with a null ParenContext when no iteration ever executed.
var r = new RegExp(unescape('%28%28%28%29%28%29%28%28%28%29%28%29%2A%2E%7C%28%29%28%68%3E%28%28%28%29%28%29%28%29%28%29%29%2F%6C%2E%28%1F%28%29%28%2D%28%00%5B%5C%44%C3%5D%6F%5E%8E%28%29%29%28%29%21%29%28%29%29%29%38%7E%29%40%77%EB%29%2A%3F%28%28%3F%3A%2E%29%3F%3F%29%29%5B%0A%61%2D%63%5D%29%29%7B%33%7D%63%70'));
r.exec('aaaaaaa');
