// Object.prototype.hasOwnProperty

function MyClass()
{
  this.y = 2;
}

MyClass.prototype = { x : 1 };

var sub = new MyClass();
shouldBe("sub.x","1");
shouldBe("sub.y","2");
shouldBe("sub.hasOwnProperty('x')","false");
shouldBe("sub.hasOwnProperty('y')","true");
sub.x = 6;
shouldBe("sub.x","6");
shouldBe("sub.hasOwnProperty('x')","true");
delete sub.y;
shouldBe("sub.y","undefined");
shouldBe("sub.hasOwnProperty('y')","false");



// Object.prototype.isPrototypeOf

function Class1() {}
function Class2() {}
function Class3() {}

Class1.prototype = new Object();
Class1.prototype.hasClass1 = true;
Class2.prototype = new Class1();
Class2.prototype.hasClass2 = true;
Class3.prototype = new Class2();
Class3.prototype.hasClass3 = true;

var obj = new Class3();
shouldBe("obj.hasClass1","true");
shouldBe("obj.hasClass2","true");
shouldBe("obj.hasClass3","true");

shouldBe("Class1.prototype.isPrototypeOf(obj)","true");
shouldBe("Class2.prototype.isPrototypeOf(obj)","true");
shouldBe("Class3.prototype.isPrototypeOf(obj)","true");
shouldBe("obj.isPrototypeOf(Class1.prototype)","false");
shouldBe("obj.isPrototypeOf(Class2.prototype)","false");
shouldBe("obj.isPrototypeOf(Class3.prototype)","false");

shouldBe("Class1.prototype.isPrototypeOf(Class2.prototype)","true");
shouldBe("Class2.prototype.isPrototypeOf(Class1.prototype)","false");
shouldBe("Class1.prototype.isPrototypeOf(Class3.prototype)","true");
shouldBe("Class3.prototype.isPrototypeOf(Class1.prototype)","false");
shouldBe("Class2.prototype.isPrototypeOf(Class3.prototype)","true");
shouldBe("Class3.prototype.isPrototypeOf(Class2.prototype)","false");

shouldBeUndefined("Class1.prototype.prototype");

function checkEnumerable(obj,property)
{
  for (var propname in obj) {
    if (propname == property)
      return true;
  }
  return false;
}

function myfunc(a,b,c)
{
}
myfunc.someproperty = 4;

shouldBe("myfunc.length","3");
shouldBe("myfunc.someproperty","4");
shouldBe("myfunc.propertyIsEnumerable('length')","false");
shouldBe("myfunc.propertyIsEnumerable('someproperty')","true");
shouldBe("checkEnumerable(myfunc,'length')","false");
shouldBe("checkEnumerable(myfunc,'someproperty')","true");
successfullyParsed = true
