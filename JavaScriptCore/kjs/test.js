var i = 0;

function sum(a, b)
{
 debug("inside test()");
 i = i + 1;
 debug(a);
 debug(b);
 return a + b;
}

s = sum(10, sum(20, 30));
debug("s = " + s);
debug("i = " + i);

var a = new Array(11, 22, 33, 44);
a.length = 2;
a[4] = 'apple';

for(i = 0; i != a.length; i++)
  debug("a[" + i + "] = " + a[i]);

var b = new Boolean(1==1);
b.toString=Object.prototype.toString;
debug("b = " + b.toString());

// regular expression
rx = /b*c/;
debug(rx.exec("abbbcd"));
