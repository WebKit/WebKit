description('Test the behavior of multiple base elements.');

var originalBase = document.URL.replace(/[^/]*$/, "");

function clean(url)
{
    if (url.length < originalBase.length)
        return url;
    if (url.substring(0, originalBase.length) !== originalBase)
        return url;
    return "http://originalbase.com/" + url.substring(originalBase.length);
}

var anchor = document.createElement('a');
anchor.href = "file";

document.body.appendChild(anchor);

shouldBe("clean(anchor.href)", "'http://originalbase.com/file'");

var base = document.createElement('base');
base.href = "http://domain.com/base/";

shouldBe("document.head.appendChild(base), clean(anchor.href)", "'http://domain.com/base/file'");
shouldBe("base.href = 'http://domain.com/base-changed/', clean(anchor.href)", "'http://domain.com/base-changed/file'");
shouldBe("document.head.removeChild(base), clean(anchor.href)", "'http://originalbase.com/file'");

base.href = "http://domain.com/base/";

var base2 = document.createElement('base');
base2.href = "http://domain.com/base2/";

var base3 = document.createElement('base');
base3.href = "http://domain.com/base3/";

shouldBe("document.head.appendChild(base), document.head.appendChild(base2), clean(anchor.href)", "'http://domain.com/base/file'");
shouldBe("base.removeAttribute('href'), clean(anchor.href)", "'http://domain.com/base2/file'");
shouldBe("document.head.appendChild(base3), clean(anchor.href)", "'http://domain.com/base2/file'");

document.head.removeChild(base);
document.head.removeChild(base2);
document.head.removeChild(base3);
