var text = unescape('%28%3F%3C%6C%3E%53%29%7C%28%28%28%28%28%28%28%3F%3C%6C%3E%28%28%28%28%29%29%29%7C%29%7B%31%7D%5C%31%2E%29%7B%30%32%7D%12%2B%29%7B%29%7B%32%29%2B%29%7B%30%32%7D%1C%29%7B%29%2D');
var r = new RegExp(text, 'ims');

'aabbbccc'.match(r);
'aabbbccc'.replace(r, '$1,$2');
r.exec('caffeeeee');
