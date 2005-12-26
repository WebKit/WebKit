///////////////////////////////////////////////////////

function Square(x)
{
  this.x = x;
}

new Square(0); // create prototype

function Square_area() { return this.x * this.x; }
Square.prototype.area = Square_area;
var s = new Square(3);
shouldBe("s.area()", "9");

///////////////////////////////////////////////////////

function Item(name){ 
  this.name = name;
}

function Book(name, author){
  this.base = Item;         // set Item constructor as method of Book object
  this.base(name);           // set the value of name property
  this.author = author;
}
Book.prototype = new Item;
var b = new Book("a book", "Fred");        // create object instance
//edebug(e"b.name"));
shouldBe("b.name", "'a book'");
shouldBe("b.author", "'Fred'");                  // outpus "Fred" 

///////////////////////////////////////////////////////

shouldBe("delete Boolean.prototype", "false");

successfullyParsed = true
