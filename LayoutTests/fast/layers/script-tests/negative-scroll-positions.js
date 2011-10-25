description(
"This tests that negative scrollTop and scrollLeft values are clamped to zero."
);

var scroller = document.createElement('div');
scroller.id = 'scroller';
scroller.style.overflow = 'scroll';
scroller.style.width = '200px';
scroller.style.height = '200px';

var contents = document.createElement('div');
contents.style.width = '400px';
contents.style.height = '400px';

scroller.appendChild(contents);
document.body.appendChild(scroller);

function attemptScroll(x, y)
{
  scroller.scrollTop = x;
  scroller.scrollLeft = y;
  return scroller.scrollTop + ',' + scroller.scrollLeft;
}

shouldBe("attemptScroll(0, 0)", "'0,0'");
shouldBe("attemptScroll(50, 50)", "'50,50'");
shouldBe("attemptScroll(-50, -50)", "'0,0'");
