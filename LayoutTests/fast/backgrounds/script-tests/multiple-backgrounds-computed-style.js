description("This tests checks that all of the input values for background-repeat parse correctly.");

function test(property, value)
{
    var div = document.createElement("div");
    div.setAttribute("style", value);
    document.body.appendChild(div);
    
    var result = window.getComputedStyle(div, property)[property];
    document.body.removeChild(div);
    return result;
}

// shorthands
shouldBe('test("backgroundImage", "background: none 10px 10px, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC) 20px 20px;")',
  '"none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC)"');
shouldBe('test("backgroundPosition", "background: none 10px 10px, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC) 20px 20px;")', '"10px 10px, 20px 20px"');

// background longhands
shouldBe('test("backgroundImage", "background-image: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC), none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC)")',
  '"url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC), none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC)"');
shouldBe('test("backgroundRepeat", "background-repeat: repeat-x, repeat-y, repeat, no-repeat;")', '"repeat-x, repeat-y, repeat, no-repeat"');
shouldBe('test("backgroundSize", "background-size: contain, cover, 20px 10%;")', '"contain, cover, 20px 10%"');
shouldBe('test("webkitBackgroundSize", "-webkit-background-size: contain, cover, 20px 10%;")', '"contain, cover, 20px 10%"');
shouldBe('test("webkitBackgroundComposite", "-webkit-background-composite: source-over, copy, destination-in")', '"source-over, copy, destination-in"');
shouldBe('test("backgroundAttachment", "background-attachment: fixed, scroll, local;")', '"fixed, scroll, local"');
shouldBe('test("backgroundClip", "background-clip: border-box, padding-box;")', '"border-box, padding-box"');
shouldBe('test("webkitBackgroundClip", "-webkit-background-clip: border-box, padding-box;")', '"border-box, padding-box"');
shouldBe('test("backgroundOrigin", "background-origin: border-box, padding-box, content-box;")', '"border-box, padding-box, content-box"');
shouldBe('test("webkitBackgroundOrigin", "-webkit-background-origin: border-box, padding-box, content-box;")', '"border-box, padding-box, content-box"');
shouldBe('test("backgroundPosition", "background-position: 20px 30px, 10% 90%, top, left, center;")', '"20px 30px, 10% 90%, 50% 0%, 0% 50%, 50% 50%"');
shouldBe('test("backgroundPositionX", "background-position-x: 20px, 10%, right, left, center;")', '"20px, 10%, 100%, 0%, 50%"');
shouldBe('test("backgroundPositionY", "background-position-y: 20px, 10%, bottom, top, center;")', '"20px, 10%, 100%, 0%, 50%"');

// mask shorthands
shouldBe('test("webkitMaskImage", "-webkit-mask: none 10px 10px, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC) 20px 20px;")', '"none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC)"');
shouldBe('test("webkitMaskPosition", "-webkit-mask: none 10px 10px, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC) 20px 20px;")', '"10px 10px, 20px 20px"');

// mask longhands
shouldBe('test("webkitMaskImage", "-webkit-mask-image: none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC);")', '"none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC)"');
shouldBe('test("webkitMaskSize", "-webkit-mask-size: contain, cover, 20px 10%;")', '"contain, cover, 20px 10%"');
shouldBe('test("webkitMaskRepeat", "-webkit-mask-repeat: repeat-x, repeat-y, repeat, no-repeat;")', '"repeat-x, repeat-y, repeat, no-repeat"');
shouldBe('test("webkitMaskClip", "-webkit-mask-clip: border-box, padding-box;")', '"border-box, padding-box"');
shouldBe('test("webkitMaskOrigin", "-webkit-mask-origin: border-box, padding-box, content-box;")', '"border-box, padding-box, content-box"');
shouldBe('test("webkitMaskPosition", "-webkit-mask-position: 20px 30px, 10% 90%, top, left, center;")', '"20px 30px, 10% 90%, 50% 0%, 0% 50%, 50% 50%"');
shouldBe('test("webkitMaskPositionX", "-webkit-mask-position-x: 20px, 10%, right, left, center;")', '"20px, 10%, 100%, 0%, 50%"');
shouldBe('test("webkitMaskPositionY", "-webkit-mask-position-y: 20px, 10%, bottom, top, center;")', '"20px, 10%, 100%, 0%, 50%"');
shouldBe('test("webkitMaskComposite", "-webkit-mask-composite: source-over, copy, destination-in")', '"source-over, copy, destination-in"');

var successfullyParsed = true;
