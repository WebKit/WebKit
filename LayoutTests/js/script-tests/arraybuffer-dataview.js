description(
    "This test checks that DataView methods work at different alignments and with both endians."
);

function paddedHex(v)
{
    var result = ""

    if (v < 16)
        result = "0"
    result += v.toString(16);

    return result
}

function byteString(view, start, len)
{
    if (start < 0 || len < 0 || start + len > view.byteLength)
        return "Undefined"

    result = ""
    for (var i = 0; i < len; i++) {
	if (i)
	    result += " "
        result += paddedHex(view.getUint8(start + i))
    }

    return result
}

function clearView(view)
{
    for (var i = 0; i < view.byteLength; i++)
	view.setUint8(i, 0);
}

var buffer = new ArrayBuffer(16);
var view = new DataView(buffer, 0);

for (var i = 0; i < 8; i++) {
    clearView(view)
    view.setInt8(i, 22)
    view.setUint32(i + 1, 0x1badface)
    shouldBe("byteString(view, " + i + ", 5)", "'16 1b ad fa ce'")

    clearView(view)
    view.setInt8(i, 0x12)
    view.setInt8(i + 1, 0x34)
    view.setInt8(i + 2, 0x56)
    view.setInt8(i + 3, 0x78)
    shouldBe("byteString(view, " + i + ", 4)", "'12 34 56 78'")
    shouldBe("view.getInt16(" + i + ").toString(16)", "'1234'")
    shouldBe("view.getInt16(i, true).toString(16)", "'3412'")
    shouldBe("view.getInt32(" + i + ").toString(16)", "'12345678'")
    shouldBe("view.getInt32(i, true).toString(16)", "'78563412'")

    clearView(view)
    view.setInt32(i, -1 | 0)
    shouldBe("byteString(view, " + i + ", 4)", "'ff ff ff ff'")
    shouldBe("view.getInt8(" + i + ")", "-1")
    shouldBe("view.getUint8(" + i + ")", "255")
    shouldBe("view.getInt16(" + i + ")", "-1")
    shouldBe("view.getUint16(" + i + ")", "65535")
    shouldBe("view.getInt32(" + i + ")", "-1")
    shouldBe("view.getUint32(" + i + ")", "4294967295")

    clearView(view)
    view.setInt8(i, -1 | 0)
    shouldBeTrue("view.getInt8(" + i + ") < 0")
    shouldBeTrue("view.getInt8(" + i + ",true) < 0")
    shouldBeTrue("view.getInt16(" + i + ") < 0")
    shouldBeFalse("view.getInt16(" + i + ",true) < 0")
    shouldBeTrue("view.getInt32(" + i + ") < 0")
    shouldBeFalse("view.getInt32(" + i + ",true) < 0")

    clearView(view)
    view.setUint16(i, 0x4000)
    shouldBe("view.getFloat32(" + i + ")", "2")
    shouldBeTrue("view.getFloat32(" + i + ",true) != 2.0")

    clearView(view)
    view.setUint16(i, 0xc000)
    shouldBe("view.getFloat32(" + i + ")", "-2")
    shouldBeTrue("view.getFloat32(" + i + ",true) != -2.0")

    clearView(view)
    view.setUint32(i, 0x3fb504f3)
    shouldBeTrue("Math.abs(view.getFloat32(" + i + ") - Math.SQRT2) < 0.0000001")
    shouldBeTrue("Math.abs(view.getFloat32(" + i + ",true) - Math.SQRT2) > 0.0000001")

    clearView(view)
    view.setUint8(i, 0x7f)
    view.setUint8(i + 1, 0x80)
    view.setUint8(i + 3, 0x01)
    shouldBeNaN("view.getFloat32(" + i + ")")
    shouldBeFalse("isNaN(view.getFloat32(" + i + ",true))")

    clearView(view)
    view.setUint8(i, 0xff)
    view.setUint8(i + 1, 0xc0)
    shouldBeNaN("view.getFloat32(" + i + ")")
    shouldBeFalse("isNaN(view.getFloat32(" + i + ",true))")

    clearView(view)
    view.setUint16(i, 0x3ff0)
    shouldBe("view.getFloat64(" + i + ")", "1")
    shouldBeTrue("view.getFloat64(" + i + ",true) != 1")

    clearView(view)
    view.setUint16(i, 0xbff0)
    shouldBe("view.getFloat64(" + i + ")", "-1")
    shouldBeTrue("view.getFloat64(" + i + ",true) != -1")

    clearView(view)
    view.setUint16(i, 0x4009)
    view.setUint16(i + 2, 0x21fb)
    view.setUint16(i + 4, 0x5444)
    view.setUint16(i + 6, 0x2d18)
    shouldBeTrue("Math.abs(view.getFloat64(" + i + ") - Math.PI) < 0.000000001")
}
