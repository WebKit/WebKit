// This test passes if it does not fail assertions on a debug build.
str = "Hello There Quick Brown Fox";
str.replace(/(((el)|(ui))|((Br)|(Fo)))/g, () => { });
