description(
"Reduced test case for https://bugs.webkit.org/show_bug.cgi?id=83818"
);

a = new Uint16Array(8);

b = new Uint8Array(a.buffer, 0, 2);
b[0] = 0x05;
b[1] = 0x05;

shouldBe("a[0]", "0x0505");

a.set(b);

shouldBe("a[0]", "0x0005");
shouldBe("a[1]", "0x0005");
