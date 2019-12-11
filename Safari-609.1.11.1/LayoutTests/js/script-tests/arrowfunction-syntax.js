description("Tests for ES6 arrow function calling");

var af0 = v => v + 1;
shouldBe('af0(10)', '11');

var af1 = (v) => v * 2;
shouldBe('af1(10)', '20');

var af2 = () => 1000;
shouldBe('af2(1212)', '1000' );

var a = 151;
var af2_1 = () => a;
shouldBe('af2_1(121)', 'a');

var af3 = (v, x) => v + x;
shouldBe('af3(11,12)', '23');

var afwrapper = function (cl, paramter) { return cl(paramter); };

shouldBe('afwrapper(x => 1234)', '1234');
shouldBe('afwrapper(x => 1234, 2345)', '1234' );
shouldBe('afwrapper(x => 121 + 232)', '353' );
shouldBe('afwrapper(x => 123 + 321, 9999)', '444' );
shouldBe('afwrapper(x => x + 12, 21)', '33' );
shouldBe('afwrapper((x) => x + 21, 32)', '53');
shouldBe('afwrapper(() => 100)', '100');

var ext_value = 121;
shouldBe('afwrapper(() => ext_value)', '121');
shouldBe('afwrapper(() => ext_value * 10)', '1210');
shouldBe('afwrapper((x) => ext_value * x, 30)', 'ext_value * 30');
shouldBe('afwrapper(() => 100, 11)', '100' );
shouldBe('afwrapper(() => 100 + 10)', '110');
shouldBe('afwrapper(() => 100 + 11, 12)', '111' );

var arrowFunction4 = v => v + 1, xyz1 = 10101;
shouldBe('arrowFunction4(1011)', '1012');
shouldBe('xyz1', '10101');

var afwrapper2 = function (cl, paramter1, paramter2) { return cl(paramter1, paramter2); };
shouldBe('afwrapper2((x, y) => x + y, 12 ,43)', '55');

var afArr0 = [v => v * 10];
shouldBe('afArr0[0](10)', '100');

var afArr1 = [v => v + 1, v => v + 2];
shouldBe('afArr1[0](10)', '11');
shouldBe('afArr1[1](11)', '13');

var afArr2 = [(v) => v + 1, (v) => v + 2];
shouldBe('afArr2[0](11)', '12');
shouldBe('afArr2[1](11)', '13');

var afArr3 = [() => 101, () => 12323];
shouldBe('afArr3[0](11)', '101');
shouldBe('afArr3[1](11)', '12323');

var afObj = {func : y => y + 12};
shouldBe('afObj.func(11)', '23');

var afBlock0 = () => { return 1000; };
shouldBe('afBlock0(11)', '1000');

var afBlock1 = v => { var intval = 100; return v * intval; };
shouldBe('afBlock1(11)', '1100');

var afBlock2 = (v) => { var int = 200; return v * int; };
shouldBe('afBlock2(11)', '2200');

var afBlock3 = (v, x) => { var result = x * v; return result; };
shouldBe('afBlock3(11, 12222)', '134442');

shouldBe('(function funcSelfExecAE1(value) { var f = x => x+1; return f(value);})(123);', '124');

shouldBe('(function funcSelfExecAE2(value) { var f = x => { x++; return x + 1; }; return f(value);})(123);', '125');

shouldBe('(function funcSelfExecAE3(value) { var f = (x) => { x++; return x + 1; }; return f(value);})(123);', '125');

shouldBe('(function funcSelfExecAE4(value) { var f = (x, y) => { x++; return x + y; }; return f(value, value * 2);})(123);', '370');

shouldBe('(([a, b]) => a + b)(["a_", "b_"])', '"a_b_"');
shouldBe('(({a, b}) => a + b)({a:"a_", b:"b_"})', '"a_b_"');
shouldBe('(({c:a, d:b}) => a + b)({c:"a_", d:"b_"})', '"a_b_"');
shouldBe('(({c:b, d:a}) => a + b)({c:"a_", d:"b_"})', '"b_a_"');
shouldBe('((x, y, {c:b, d:a}) => x + y + a + b)("x_", "y_", {c:"a_", d:"b_"})', '"x_y_b_a_"');
shouldBe('(({c:b, d:a}, x, y) => x + y + a + b)({c:"a_", d:"b_"}, "x_", "y_")', '"x_y_b_a_"');
shouldBe('((x, y, {c:b, d:a}, [e, f]) => x + y + a + b + e + f)("x_", "y_", {c:"a_", d:"b_"}, ["e_", "f_"])', '"x_y_b_a_e_f_"');
shouldBe('((x, y, {c:b, d:a}, [e, f], ...theArgs) => x + y + a + b + e + f + theArgs[0] + theArgs[1])("x_", "y_", {c:"a_", d:"b_"}, ["e_", "f_"], "g_", "h_")', '"x_y_b_a_e_f_g_h_"');
shouldBe("((x, y = 'default-value') => x + y)('input-value:')",'"input-value:default-value"');
shouldBe("((x, y = 'default-value') => x + y)('input-value:', undefined)",'"input-value:default-value"');
shouldBe("((x, y = 'default-value') => x + y)()",'"undefineddefault-value"');
shouldBe("((x, y = 'default-value') => x + y)('input-value-1:','input-value-2')",'"input-value-1:input-value-2"');

var arr1 = ([a, b]) => a + b;
shouldBe('arr1(["a_", "b_"])', '"a_b_"');

var arr2 = ({a, b}) => a + b;
shouldBe('arr2({a:"a_", b:"b_"})', '"a_b_"');

var arr3 = ({c:a, d:b}) => a + b;
shouldBe('arr3({c:"a_", d:"b_"})', '"a_b_"');

var arr4 = ({c:b, d:a}) => a + b;
shouldBe('arr4({c:"a_", d:"b_"})', '"b_a_"');

var arr5 = (x, y, {c:b, d:a}) => x + y + a + b;
shouldBe('arr5("x_", "y_", {c:"a_", d:"b_"})', '"x_y_b_a_"');

var arr6 = ({c:b, d:a}, x, y) => x + y + a + b;
shouldBe('arr6({c:"a_", d:"b_"}, "x_", "y_")', '"x_y_b_a_"');

var arr7 = (x, y, {c:b, d:a}, [e, f]) => x + y + a + b + e + f;
shouldBe('arr7("x_", "y_", {c:"a_", d:"b_"}, ["e_", "f_"])', '"x_y_b_a_e_f_"');

var arr8 = (x, y, {c:b, d:a}, [e, f], ...theArgs) => x + y + a + b + e + f + theArgs[0] + theArgs[1];
shouldBe('arr8("x_", "y_", {c:"a_", d:"b_"}, ["e_", "f_"], "g_", "h_")', '"x_y_b_a_e_f_g_h_"');

var arr9 = (x, y = 'default-value') => x + y;
shouldBe('arr9("input-value:")','"input-value:default-value"');
shouldBe('arr9("input-value:", undefined)','"input-value:default-value"');
shouldBe('arr9()','"undefineddefault-value"');
shouldBe('arr9("input-value-1:", "input-value-2")','"input-value-1:input-value-2"');

var successfullyParsed = true;
