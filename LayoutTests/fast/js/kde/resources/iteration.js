// 12.6.1
var count = 0;
do {
  count++;
} while (count < 10);
shouldBe("count", "10");

count = 0;
for (var i = 0; i < 10; i++) {
  if (i == 5)
    break;
  count++;
}
shouldBe("count", "5");

// 12.6.3
count = 0;
for (i = 0; i < 10; i++) {
  count++;
}
shouldBe("count", "10");

// 12.6.4
obj = new Object();
obj.a = 11;
obj.b = 22;

properties = "";
for ( prop in obj )
  properties += (prop + "=" + obj[prop] + ";");

shouldBe("properties", "'a=11;b=22;'");

// now a test verifying the order. not standardized but common.
obj.y = 33;
obj.x = 44;
properties = "";
for ( prop in obj )
  properties += prop;
// shouldBe("properties", "'abyx'");

arr = new Array;
arr[0] = 100;
arr[1] = 101;
list = "";
for ( var j in arr ) {
  list += "[" + j + "]=" + arr[j] + ";";
}
shouldBe("list","'[0]=100;[1]=101;'");

list = "";
for (var a = [1,2,3], length = a.length, i = 0; i < length; i++) {
  list += a[i];
}
shouldBe("list", "'123'");
successfullyParsed = true
