description("Test for error messages for instanceof");

shouldThrow("'instanceof' instanceof    'instanceof'");
shouldThrow("20 instanceof     'hello'  ");
shouldThrow("20 instanceof     {}  ");
shouldThrow("20 instanceof     {}.foo ");
shouldThrow("20 instanceof     true      ");
