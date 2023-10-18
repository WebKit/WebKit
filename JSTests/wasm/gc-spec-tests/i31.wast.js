// i31.wast:1
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x95\x80\x80\x80\x00\x04\x60\x01\x7f\x01\x64\x6c\x60\x01\x7f\x01\x7f\x60\x00\x01\x7f\x60\x00\x02\x7f\x7f\x03\x87\x80\x80\x80\x00\x06\x00\x01\x01\x02\x02\x03\x06\x91\x80\x80\x80\x00\x02\x64\x6c\x00\x41\x02\xfb\x1c\x0b\x64\x6c\x01\x41\x03\xfb\x1c\x0b\x07\xbf\x80\x80\x80\x00\x06\x03\x6e\x65\x77\x00\x00\x05\x67\x65\x74\x5f\x75\x00\x01\x05\x67\x65\x74\x5f\x73\x00\x02\x0a\x67\x65\x74\x5f\x75\x2d\x6e\x75\x6c\x6c\x00\x03\x0a\x67\x65\x74\x5f\x73\x2d\x6e\x75\x6c\x6c\x00\x04\x0b\x67\x65\x74\x5f\x67\x6c\x6f\x62\x61\x6c\x73\x00\x05\x0a\xcb\x80\x80\x80\x00\x06\x86\x80\x80\x80\x00\x00\x20\x00\xfb\x1c\x0b\x88\x80\x80\x80\x00\x00\x20\x00\xfb\x1c\xfb\x1e\x0b\x88\x80\x80\x80\x00\x00\x20\x00\xfb\x1c\xfb\x1d\x0b\x86\x80\x80\x80\x00\x00\xd0\x6c\xfb\x1e\x0b\x86\x80\x80\x80\x00\x00\xd0\x6c\xfb\x1e\x0b\x8a\x80\x80\x80\x00\x00\x23\x00\xfb\x1e\x23\x01\xfb\x1e\x0b");

// i31.wast:28
assert_return(() => call($1, "new", [1]), "ref.i31");

// i31.wast:30
assert_return(() => call($1, "get_u", [0]), 0);

// i31.wast:31
assert_return(() => call($1, "get_u", [100]), 100);

// i31.wast:32
assert_return(() => call($1, "get_u", [-1]), 2_147_483_647);

// i31.wast:33
assert_return(() => call($1, "get_u", [1_073_741_823]), 1_073_741_823);

// i31.wast:34
assert_return(() => call($1, "get_u", [1_073_741_824]), 1_073_741_824);

// i31.wast:35
assert_return(() => call($1, "get_u", [2_147_483_647]), 2_147_483_647);

// i31.wast:36
assert_return(() => call($1, "get_u", [-1_431_655_766]), 715_827_882);

// i31.wast:37
assert_return(() => call($1, "get_u", [-894_784_854]), 1_252_698_794);

// i31.wast:39
assert_return(() => call($1, "get_s", [0]), 0);

// i31.wast:40
assert_return(() => call($1, "get_s", [100]), 100);

// i31.wast:41
assert_return(() => call($1, "get_s", [-1]), -1);

// i31.wast:42
assert_return(() => call($1, "get_s", [1_073_741_823]), 1_073_741_823);

// i31.wast:43
assert_return(() => call($1, "get_s", [1_073_741_824]), -1_073_741_824);

// i31.wast:44
assert_return(() => call($1, "get_s", [2_147_483_647]), -1);

// i31.wast:45
assert_return(() => call($1, "get_s", [-1_431_655_766]), 715_827_882);

// i31.wast:46
assert_return(() => call($1, "get_s", [-894_784_854]), -894_784_854);

// i31.wast:48
assert_trap(() => call($1, "get_u-null", []));

// i31.wast:49
assert_trap(() => call($1, "get_s-null", []));

// i31.wast:51
assert_return(() => call($1, "get_globals", []), 2, 3);
