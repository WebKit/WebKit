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
shouldBe('test("backgroundRepeat", "background-image: none, none, none, none; background-repeat: repeat-x, repeat-y, repeat, no-repeat;")', '"repeat-x, repeat-y, repeat, no-repeat"');
shouldBe('test("backgroundSize", "background-image: none, none, none; background-size: contain, cover, 20px 10%;")', '"contain, cover, 20px 10%"');
shouldBe('test("webkitBackgroundSize", "background-image: none, none, none; -webkit-background-size: contain, cover, 20px 10%;")', '"contain, cover, 20px 10%"');
shouldBe('test("webkitBackgroundComposite", "background-image: none, none, none; -webkit-background-composite: source-over, copy, destination-in")', '"source-over, copy, destination-in"');
shouldBe('test("backgroundAttachment", "background-image: none, none, none; background-attachment: fixed, scroll, local;")', '"fixed, scroll, local"');
shouldBe('test("backgroundClip", "background-image: none, none; background-clip: border-box, padding-box;")', '"border-box, padding-box"');
shouldBe('test("webkitBackgroundClip", "background-image: none, none; -webkit-background-clip: border-box, padding-box;")', '"border-box, padding-box"');
shouldBe('test("backgroundOrigin", "background-image: none, none, none; background-origin: border-box, padding-box, content-box;")', '"border-box, padding-box, content-box"');
shouldBe('test("webkitBackgroundOrigin", "background-image: none, none, none; -webkit-background-origin: border-box, padding-box, content-box;")', '"border-box, padding-box, content-box"');
shouldBe('test("backgroundPosition", "background-image: none, none, none, none, none; background-position: 20px 30px, 10% 90%, top, left, center;")', '"20px 30px, 10% 90%, 50% 0%, 0% 50%, 50% 50%"');
shouldBe('test("backgroundPositionX", "background-image: none, none, none, none, none; background-position-x: 20px, 10%, right, left, center;")', '"20px, 10%, 100%, 0%, 50%"');
shouldBe('test("backgroundPositionY", "background-image: none, none, none, none, none; background-position-y: 20px, 10%, bottom, top, center;")', '"20px, 10%, 100%, 0%, 50%"');

// mask shorthands
shouldBe('test("webkitMaskImage", "-webkit-mask: none 10px 10px, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC) 20px 20px;")', '"none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC)"');
shouldBe('test("webkitMaskPosition", "-webkit-mask: none 10px 10px, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC) 20px 20px;")', '"10px 10px, 20px 20px"');
shouldBe('test("webkitMaskClip", "-webkit-mask: center url() content-box ")', '"content-box"');
shouldBe('test("webkitMaskClip", "-webkit-mask: content-box padding-box")', '"padding-box"');
shouldBe('test("webkitMaskClip", "-webkit-mask: border-box url() content-box")', '"content-box"');
shouldBe('test("webkitMaskClip", "-webkit-mask: url() repeat-x content-box border-box scroll ")', '"border-box"');

// mask longhands
shouldBe('test("webkitMaskImage", "-webkit-mask-image: none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC);")', '"none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR42mP4%2F58BAAT%2FAf9jgNErAAAAAElFTkSuQmCC)"');
shouldBe('test("webkitMaskSize", "-webkit-mask-image: none, none, none; -webkit-mask-size: contain, cover, 20px 10%;")', '"contain, cover, 20px 10%"');
shouldBe('test("webkitMaskRepeat", "-webkit-mask-image: none, none, none, none; -webkit-mask-repeat: repeat-x, repeat-y, repeat, no-repeat;")', '"repeat-x, repeat-y, repeat, no-repeat"');
shouldBe('test("webkitMaskClip", "-webkit-mask-image: none, none; -webkit-mask-clip: border-box, padding-box;")', '"border-box, padding-box"');
shouldBe('test("webkitMaskOrigin", "-webkit-mask-image: none, none, none; -webkit-mask-origin: border-box, padding-box, content-box;")', '"border-box, padding-box, content-box"');
shouldBe('test("webkitMaskPosition", "-webkit-mask-image: none, none, none, none, none; -webkit-mask-position: 20px 30px, 10% 90%, top, left, center;")', '"20px 30px, 10% 90%, 50% 0%, 0% 50%, 50% 50%"');
shouldBe('test("webkitMaskPositionX", "-webkit-mask-image: none, none, none, none, none; -webkit-mask-position-x: 20px, 10%, right, left, center;")', '"20px, 10%, 100%, 0%, 50%"');
shouldBe('test("webkitMaskPositionY", "-webkit-mask-image: none, none, none, none, none; -webkit-mask-position-y: 20px, 10%, bottom, top, center;")', '"20px, 10%, 100%, 0%, 50%"');
shouldBe('test("webkitMaskComposite", "-webkit-mask-image: none, none, none; -webkit-mask-composite: source-over, copy, destination-in")', '"source-over, copy, destination-in"');

var successfullyParsed = true;
