description("Test that we correctly encode patterned immediate values");

shouldBeFalse("0 >= 0x01000100")
shouldBeFalse("0 >= 0x01010000")
shouldBeFalse("0 >= 0x00000101")
shouldBeFalse("0 >= 0x00010001")
shouldBeFalse("0 >= 0x01010101")
