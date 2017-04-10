
// i32.wast:3
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x02\x7f\x7f\x01\x7f\x60\x01\x7f\x01\x7f\x03\x9e\x80\x80\x80\x00\x1d\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x07\xc5\x81\x80\x80\x00\x1d\x03\x61\x64\x64\x00\x00\x03\x73\x75\x62\x00\x01\x03\x6d\x75\x6c\x00\x02\x05\x64\x69\x76\x5f\x73\x00\x03\x05\x64\x69\x76\x5f\x75\x00\x04\x05\x72\x65\x6d\x5f\x73\x00\x05\x05\x72\x65\x6d\x5f\x75\x00\x06\x03\x61\x6e\x64\x00\x07\x02\x6f\x72\x00\x08\x03\x78\x6f\x72\x00\x09\x03\x73\x68\x6c\x00\x0a\x05\x73\x68\x72\x5f\x73\x00\x0b\x05\x73\x68\x72\x5f\x75\x00\x0c\x04\x72\x6f\x74\x6c\x00\x0d\x04\x72\x6f\x74\x72\x00\x0e\x03\x63\x6c\x7a\x00\x0f\x03\x63\x74\x7a\x00\x10\x06\x70\x6f\x70\x63\x6e\x74\x00\x11\x03\x65\x71\x7a\x00\x12\x02\x65\x71\x00\x13\x02\x6e\x65\x00\x14\x04\x6c\x74\x5f\x73\x00\x15\x04\x6c\x74\x5f\x75\x00\x16\x04\x6c\x65\x5f\x73\x00\x17\x04\x6c\x65\x5f\x75\x00\x18\x04\x67\x74\x5f\x73\x00\x19\x04\x67\x74\x5f\x75\x00\x1a\x04\x67\x65\x5f\x73\x00\x1b\x04\x67\x65\x5f\x75\x00\x1c\x0a\xd5\x82\x80\x80\x00\x1d\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x6a\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x6b\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x6c\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x6d\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x6e\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x6f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x70\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x71\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x72\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x73\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x74\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x75\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x76\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x77\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x78\x0b\x85\x80\x80\x80\x00\x00\x20\x00\x67\x0b\x85\x80\x80\x80\x00\x00\x20\x00\x68\x0b\x85\x80\x80\x80\x00\x00\x20\x00\x69\x0b\x85\x80\x80\x80\x00\x00\x20\x00\x45\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x46\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x47\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x48\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x49\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x4c\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x4d\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x4a\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x4b\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x4e\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x20\x01\x4f\x0b");

// i32.wast:35
assert_return(() => call($1, "add", [1, 1]), 2);

// i32.wast:36
assert_return(() => call($1, "add", [1, 0]), 1);

// i32.wast:37
assert_return(() => call($1, "add", [-1, -1]), -2);

// i32.wast:38
assert_return(() => call($1, "add", [-1, 1]), 0);

// i32.wast:39
assert_return(() => call($1, "add", [2147483647, 1]), -2147483648);

// i32.wast:40
assert_return(() => call($1, "add", [-2147483648, -1]), 2147483647);

// i32.wast:41
assert_return(() => call($1, "add", [-2147483648, -2147483648]), 0);

// i32.wast:42
assert_return(() => call($1, "add", [1073741823, 1]), 1073741824);

// i32.wast:44
assert_return(() => call($1, "sub", [1, 1]), 0);

// i32.wast:45
assert_return(() => call($1, "sub", [1, 0]), 1);

// i32.wast:46
assert_return(() => call($1, "sub", [-1, -1]), 0);

// i32.wast:47
assert_return(() => call($1, "sub", [2147483647, -1]), -2147483648);

// i32.wast:48
assert_return(() => call($1, "sub", [-2147483648, 1]), 2147483647);

// i32.wast:49
assert_return(() => call($1, "sub", [-2147483648, -2147483648]), 0);

// i32.wast:50
assert_return(() => call($1, "sub", [1073741823, -1]), 1073741824);

// i32.wast:52
assert_return(() => call($1, "mul", [1, 1]), 1);

// i32.wast:53
assert_return(() => call($1, "mul", [1, 0]), 0);

// i32.wast:54
assert_return(() => call($1, "mul", [-1, -1]), 1);

// i32.wast:55
assert_return(() => call($1, "mul", [268435456, 4096]), 0);

// i32.wast:56
assert_return(() => call($1, "mul", [-2147483648, 0]), 0);

// i32.wast:57
assert_return(() => call($1, "mul", [-2147483648, -1]), -2147483648);

// i32.wast:58
assert_return(() => call($1, "mul", [2147483647, -1]), -2147483647);

// i32.wast:59
assert_return(() => call($1, "mul", [19088743, 1985229328]), 898528368);

// i32.wast:60
assert_return(() => call($1, "mul", [2147483647, 2147483647]), 1);

// i32.wast:62
assert_trap(() => call($1, "div_s", [1, 0]));

// i32.wast:63
assert_trap(() => call($1, "div_s", [0, 0]));

// i32.wast:64
assert_trap(() => call($1, "div_s", [-2147483648, -1]));

// i32.wast:65
assert_return(() => call($1, "div_s", [1, 1]), 1);

// i32.wast:66
assert_return(() => call($1, "div_s", [0, 1]), 0);

// i32.wast:67
assert_return(() => call($1, "div_s", [0, -1]), 0);

// i32.wast:68
assert_return(() => call($1, "div_s", [-1, -1]), 1);

// i32.wast:69
assert_return(() => call($1, "div_s", [-2147483648, 2]), -1073741824);

// i32.wast:70
assert_return(() => call($1, "div_s", [-2147483647, 1000]), -2147483);

// i32.wast:71
assert_return(() => call($1, "div_s", [5, 2]), 2);

// i32.wast:72
assert_return(() => call($1, "div_s", [-5, 2]), -2);

// i32.wast:73
assert_return(() => call($1, "div_s", [5, -2]), -2);

// i32.wast:74
assert_return(() => call($1, "div_s", [-5, -2]), 2);

// i32.wast:75
assert_return(() => call($1, "div_s", [7, 3]), 2);

// i32.wast:76
assert_return(() => call($1, "div_s", [-7, 3]), -2);

// i32.wast:77
assert_return(() => call($1, "div_s", [7, -3]), -2);

// i32.wast:78
assert_return(() => call($1, "div_s", [-7, -3]), 2);

// i32.wast:79
assert_return(() => call($1, "div_s", [11, 5]), 2);

// i32.wast:80
assert_return(() => call($1, "div_s", [17, 7]), 2);

// i32.wast:82
assert_trap(() => call($1, "div_u", [1, 0]));

// i32.wast:83
assert_trap(() => call($1, "div_u", [0, 0]));

// i32.wast:84
assert_return(() => call($1, "div_u", [1, 1]), 1);

// i32.wast:85
assert_return(() => call($1, "div_u", [0, 1]), 0);

// i32.wast:86
assert_return(() => call($1, "div_u", [-1, -1]), 1);

// i32.wast:87
assert_return(() => call($1, "div_u", [-2147483648, -1]), 0);

// i32.wast:88
assert_return(() => call($1, "div_u", [-2147483648, 2]), 1073741824);

// i32.wast:89
assert_return(() => call($1, "div_u", [-1880092688, 65537]), 36847);

// i32.wast:90
assert_return(() => call($1, "div_u", [-2147483647, 1000]), 2147483);

// i32.wast:91
assert_return(() => call($1, "div_u", [5, 2]), 2);

// i32.wast:92
assert_return(() => call($1, "div_u", [-5, 2]), 2147483645);

// i32.wast:93
assert_return(() => call($1, "div_u", [5, -2]), 0);

// i32.wast:94
assert_return(() => call($1, "div_u", [-5, -2]), 0);

// i32.wast:95
assert_return(() => call($1, "div_u", [7, 3]), 2);

// i32.wast:96
assert_return(() => call($1, "div_u", [11, 5]), 2);

// i32.wast:97
assert_return(() => call($1, "div_u", [17, 7]), 2);

// i32.wast:99
assert_trap(() => call($1, "rem_s", [1, 0]));

// i32.wast:100
assert_trap(() => call($1, "rem_s", [0, 0]));

// i32.wast:101
assert_return(() => call($1, "rem_s", [2147483647, -1]), 0);

// i32.wast:102
assert_return(() => call($1, "rem_s", [1, 1]), 0);

// i32.wast:103
assert_return(() => call($1, "rem_s", [0, 1]), 0);

// i32.wast:104
assert_return(() => call($1, "rem_s", [0, -1]), 0);

// i32.wast:105
assert_return(() => call($1, "rem_s", [-1, -1]), 0);

// i32.wast:106
assert_return(() => call($1, "rem_s", [-2147483648, -1]), 0);

// i32.wast:107
assert_return(() => call($1, "rem_s", [-2147483648, 2]), 0);

// i32.wast:108
assert_return(() => call($1, "rem_s", [-2147483647, 1000]), -647);

// i32.wast:109
assert_return(() => call($1, "rem_s", [5, 2]), 1);

// i32.wast:110
assert_return(() => call($1, "rem_s", [-5, 2]), -1);

// i32.wast:111
assert_return(() => call($1, "rem_s", [5, -2]), 1);

// i32.wast:112
assert_return(() => call($1, "rem_s", [-5, -2]), -1);

// i32.wast:113
assert_return(() => call($1, "rem_s", [7, 3]), 1);

// i32.wast:114
assert_return(() => call($1, "rem_s", [-7, 3]), -1);

// i32.wast:115
assert_return(() => call($1, "rem_s", [7, -3]), 1);

// i32.wast:116
assert_return(() => call($1, "rem_s", [-7, -3]), -1);

// i32.wast:117
assert_return(() => call($1, "rem_s", [11, 5]), 1);

// i32.wast:118
assert_return(() => call($1, "rem_s", [17, 7]), 3);

// i32.wast:120
assert_trap(() => call($1, "rem_u", [1, 0]));

// i32.wast:121
assert_trap(() => call($1, "rem_u", [0, 0]));

// i32.wast:122
assert_return(() => call($1, "rem_u", [1, 1]), 0);

// i32.wast:123
assert_return(() => call($1, "rem_u", [0, 1]), 0);

// i32.wast:124
assert_return(() => call($1, "rem_u", [-1, -1]), 0);

// i32.wast:125
assert_return(() => call($1, "rem_u", [-2147483648, -1]), -2147483648);

// i32.wast:126
assert_return(() => call($1, "rem_u", [-2147483648, 2]), 0);

// i32.wast:127
assert_return(() => call($1, "rem_u", [-1880092688, 65537]), 32769);

// i32.wast:128
assert_return(() => call($1, "rem_u", [-2147483647, 1000]), 649);

// i32.wast:129
assert_return(() => call($1, "rem_u", [5, 2]), 1);

// i32.wast:130
assert_return(() => call($1, "rem_u", [-5, 2]), 1);

// i32.wast:131
assert_return(() => call($1, "rem_u", [5, -2]), 5);

// i32.wast:132
assert_return(() => call($1, "rem_u", [-5, -2]), -5);

// i32.wast:133
assert_return(() => call($1, "rem_u", [7, 3]), 1);

// i32.wast:134
assert_return(() => call($1, "rem_u", [11, 5]), 1);

// i32.wast:135
assert_return(() => call($1, "rem_u", [17, 7]), 3);

// i32.wast:137
assert_return(() => call($1, "and", [1, 0]), 0);

// i32.wast:138
assert_return(() => call($1, "and", [0, 1]), 0);

// i32.wast:139
assert_return(() => call($1, "and", [1, 1]), 1);

// i32.wast:140
assert_return(() => call($1, "and", [0, 0]), 0);

// i32.wast:141
assert_return(() => call($1, "and", [2147483647, -2147483648]), 0);

// i32.wast:142
assert_return(() => call($1, "and", [2147483647, -1]), 2147483647);

// i32.wast:143
assert_return(() => call($1, "and", [-252641281, -3856]), -252645136);

// i32.wast:144
assert_return(() => call($1, "and", [-1, -1]), -1);

// i32.wast:146
assert_return(() => call($1, "or", [1, 0]), 1);

// i32.wast:147
assert_return(() => call($1, "or", [0, 1]), 1);

// i32.wast:148
assert_return(() => call($1, "or", [1, 1]), 1);

// i32.wast:149
assert_return(() => call($1, "or", [0, 0]), 0);

// i32.wast:150
assert_return(() => call($1, "or", [2147483647, -2147483648]), -1);

// i32.wast:151
assert_return(() => call($1, "or", [-2147483648, 0]), -2147483648);

// i32.wast:152
assert_return(() => call($1, "or", [-252641281, -3856]), -1);

// i32.wast:153
assert_return(() => call($1, "or", [-1, -1]), -1);

// i32.wast:155
assert_return(() => call($1, "xor", [1, 0]), 1);

// i32.wast:156
assert_return(() => call($1, "xor", [0, 1]), 1);

// i32.wast:157
assert_return(() => call($1, "xor", [1, 1]), 0);

// i32.wast:158
assert_return(() => call($1, "xor", [0, 0]), 0);

// i32.wast:159
assert_return(() => call($1, "xor", [2147483647, -2147483648]), -1);

// i32.wast:160
assert_return(() => call($1, "xor", [-2147483648, 0]), -2147483648);

// i32.wast:161
assert_return(() => call($1, "xor", [-1, -2147483648]), 2147483647);

// i32.wast:162
assert_return(() => call($1, "xor", [-1, 2147483647]), -2147483648);

// i32.wast:163
assert_return(() => call($1, "xor", [-252641281, -3856]), 252645135);

// i32.wast:164
assert_return(() => call($1, "xor", [-1, -1]), 0);

// i32.wast:166
assert_return(() => call($1, "shl", [1, 1]), 2);

// i32.wast:167
assert_return(() => call($1, "shl", [1, 0]), 1);

// i32.wast:168
assert_return(() => call($1, "shl", [2147483647, 1]), -2);

// i32.wast:169
assert_return(() => call($1, "shl", [-1, 1]), -2);

// i32.wast:170
assert_return(() => call($1, "shl", [-2147483648, 1]), 0);

// i32.wast:171
assert_return(() => call($1, "shl", [1073741824, 1]), -2147483648);

// i32.wast:172
assert_return(() => call($1, "shl", [1, 31]), -2147483648);

// i32.wast:173
assert_return(() => call($1, "shl", [1, 32]), 1);

// i32.wast:174
assert_return(() => call($1, "shl", [1, 33]), 2);

// i32.wast:175
assert_return(() => call($1, "shl", [1, -1]), -2147483648);

// i32.wast:176
assert_return(() => call($1, "shl", [1, 2147483647]), -2147483648);

// i32.wast:178
assert_return(() => call($1, "shr_s", [1, 1]), 0);

// i32.wast:179
assert_return(() => call($1, "shr_s", [1, 0]), 1);

// i32.wast:180
assert_return(() => call($1, "shr_s", [-1, 1]), -1);

// i32.wast:181
assert_return(() => call($1, "shr_s", [2147483647, 1]), 1073741823);

// i32.wast:182
assert_return(() => call($1, "shr_s", [-2147483648, 1]), -1073741824);

// i32.wast:183
assert_return(() => call($1, "shr_s", [1073741824, 1]), 536870912);

// i32.wast:184
assert_return(() => call($1, "shr_s", [1, 32]), 1);

// i32.wast:185
assert_return(() => call($1, "shr_s", [1, 33]), 0);

// i32.wast:186
assert_return(() => call($1, "shr_s", [1, -1]), 0);

// i32.wast:187
assert_return(() => call($1, "shr_s", [1, 2147483647]), 0);

// i32.wast:188
assert_return(() => call($1, "shr_s", [1, -2147483648]), 1);

// i32.wast:189
assert_return(() => call($1, "shr_s", [-2147483648, 31]), -1);

// i32.wast:190
assert_return(() => call($1, "shr_s", [-1, 32]), -1);

// i32.wast:191
assert_return(() => call($1, "shr_s", [-1, 33]), -1);

// i32.wast:192
assert_return(() => call($1, "shr_s", [-1, -1]), -1);

// i32.wast:193
assert_return(() => call($1, "shr_s", [-1, 2147483647]), -1);

// i32.wast:194
assert_return(() => call($1, "shr_s", [-1, -2147483648]), -1);

// i32.wast:196
assert_return(() => call($1, "shr_u", [1, 1]), 0);

// i32.wast:197
assert_return(() => call($1, "shr_u", [1, 0]), 1);

// i32.wast:198
assert_return(() => call($1, "shr_u", [-1, 1]), 2147483647);

// i32.wast:199
assert_return(() => call($1, "shr_u", [2147483647, 1]), 1073741823);

// i32.wast:200
assert_return(() => call($1, "shr_u", [-2147483648, 1]), 1073741824);

// i32.wast:201
assert_return(() => call($1, "shr_u", [1073741824, 1]), 536870912);

// i32.wast:202
assert_return(() => call($1, "shr_u", [1, 32]), 1);

// i32.wast:203
assert_return(() => call($1, "shr_u", [1, 33]), 0);

// i32.wast:204
assert_return(() => call($1, "shr_u", [1, -1]), 0);

// i32.wast:205
assert_return(() => call($1, "shr_u", [1, 2147483647]), 0);

// i32.wast:206
assert_return(() => call($1, "shr_u", [1, -2147483648]), 1);

// i32.wast:207
assert_return(() => call($1, "shr_u", [-2147483648, 31]), 1);

// i32.wast:208
assert_return(() => call($1, "shr_u", [-1, 32]), -1);

// i32.wast:209
assert_return(() => call($1, "shr_u", [-1, 33]), 2147483647);

// i32.wast:210
assert_return(() => call($1, "shr_u", [-1, -1]), 1);

// i32.wast:211
assert_return(() => call($1, "shr_u", [-1, 2147483647]), 1);

// i32.wast:212
assert_return(() => call($1, "shr_u", [-1, -2147483648]), -1);

// i32.wast:214
assert_return(() => call($1, "rotl", [1, 1]), 2);

// i32.wast:215
assert_return(() => call($1, "rotl", [1, 0]), 1);

// i32.wast:216
assert_return(() => call($1, "rotl", [-1, 1]), -1);

// i32.wast:217
assert_return(() => call($1, "rotl", [1, 32]), 1);

// i32.wast:218
assert_return(() => call($1, "rotl", [-1412589450, 1]), 1469788397);

// i32.wast:219
assert_return(() => call($1, "rotl", [-33498112, 4]), -535969777);

// i32.wast:220
assert_return(() => call($1, "rotl", [-1329474845, 5]), 406477942);

// i32.wast:221
assert_return(() => call($1, "rotl", [32768, 37]), 1048576);

// i32.wast:222
assert_return(() => call($1, "rotl", [-1329474845, 65285]), 406477942);

// i32.wast:223
assert_return(() => call($1, "rotl", [1989852383, -19]), 1469837011);

// i32.wast:224
assert_return(() => call($1, "rotl", [1989852383, -2147483635]), 1469837011);

// i32.wast:225
assert_return(() => call($1, "rotl", [1, 31]), -2147483648);

// i32.wast:226
assert_return(() => call($1, "rotl", [-2147483648, 1]), 1);

// i32.wast:228
assert_return(() => call($1, "rotr", [1, 1]), -2147483648);

// i32.wast:229
assert_return(() => call($1, "rotr", [1, 0]), 1);

// i32.wast:230
assert_return(() => call($1, "rotr", [-1, 1]), -1);

// i32.wast:231
assert_return(() => call($1, "rotr", [1, 32]), 1);

// i32.wast:232
assert_return(() => call($1, "rotr", [-16724992, 1]), 2139121152);

// i32.wast:233
assert_return(() => call($1, "rotr", [524288, 4]), 32768);

// i32.wast:234
assert_return(() => call($1, "rotr", [-1329474845, 5]), 495324823);

// i32.wast:235
assert_return(() => call($1, "rotr", [32768, 37]), 1024);

// i32.wast:236
assert_return(() => call($1, "rotr", [-1329474845, 65285]), 495324823);

// i32.wast:237
assert_return(() => call($1, "rotr", [1989852383, -19]), -419711787);

// i32.wast:238
assert_return(() => call($1, "rotr", [1989852383, -2147483635]), -419711787);

// i32.wast:239
assert_return(() => call($1, "rotr", [1, 31]), 2);

// i32.wast:240
assert_return(() => call($1, "rotr", [-2147483648, 31]), 1);

// i32.wast:242
assert_return(() => call($1, "clz", [-1]), 0);

// i32.wast:243
assert_return(() => call($1, "clz", [0]), 32);

// i32.wast:244
assert_return(() => call($1, "clz", [32768]), 16);

// i32.wast:245
assert_return(() => call($1, "clz", [255]), 24);

// i32.wast:246
assert_return(() => call($1, "clz", [-2147483648]), 0);

// i32.wast:247
assert_return(() => call($1, "clz", [1]), 31);

// i32.wast:248
assert_return(() => call($1, "clz", [2]), 30);

// i32.wast:249
assert_return(() => call($1, "clz", [2147483647]), 1);

// i32.wast:251
assert_return(() => call($1, "ctz", [-1]), 0);

// i32.wast:252
assert_return(() => call($1, "ctz", [0]), 32);

// i32.wast:253
assert_return(() => call($1, "ctz", [32768]), 15);

// i32.wast:254
assert_return(() => call($1, "ctz", [65536]), 16);

// i32.wast:255
assert_return(() => call($1, "ctz", [-2147483648]), 31);

// i32.wast:256
assert_return(() => call($1, "ctz", [2147483647]), 0);

// i32.wast:258
assert_return(() => call($1, "popcnt", [-1]), 32);

// i32.wast:259
assert_return(() => call($1, "popcnt", [0]), 0);

// i32.wast:260
assert_return(() => call($1, "popcnt", [32768]), 1);

// i32.wast:261
assert_return(() => call($1, "popcnt", [-2147450880]), 2);

// i32.wast:262
assert_return(() => call($1, "popcnt", [2147483647]), 31);

// i32.wast:263
assert_return(() => call($1, "popcnt", [-1431655766]), 16);

// i32.wast:264
assert_return(() => call($1, "popcnt", [1431655765]), 16);

// i32.wast:265
assert_return(() => call($1, "popcnt", [-559038737]), 24);

// i32.wast:267
assert_return(() => call($1, "eqz", [0]), 1);

// i32.wast:268
assert_return(() => call($1, "eqz", [1]), 0);

// i32.wast:269
assert_return(() => call($1, "eqz", [-2147483648]), 0);

// i32.wast:270
assert_return(() => call($1, "eqz", [2147483647]), 0);

// i32.wast:271
assert_return(() => call($1, "eqz", [-1]), 0);

// i32.wast:273
assert_return(() => call($1, "eq", [0, 0]), 1);

// i32.wast:274
assert_return(() => call($1, "eq", [1, 1]), 1);

// i32.wast:275
assert_return(() => call($1, "eq", [-1, 1]), 0);

// i32.wast:276
assert_return(() => call($1, "eq", [-2147483648, -2147483648]), 1);

// i32.wast:277
assert_return(() => call($1, "eq", [2147483647, 2147483647]), 1);

// i32.wast:278
assert_return(() => call($1, "eq", [-1, -1]), 1);

// i32.wast:279
assert_return(() => call($1, "eq", [1, 0]), 0);

// i32.wast:280
assert_return(() => call($1, "eq", [0, 1]), 0);

// i32.wast:281
assert_return(() => call($1, "eq", [-2147483648, 0]), 0);

// i32.wast:282
assert_return(() => call($1, "eq", [0, -2147483648]), 0);

// i32.wast:283
assert_return(() => call($1, "eq", [-2147483648, -1]), 0);

// i32.wast:284
assert_return(() => call($1, "eq", [-1, -2147483648]), 0);

// i32.wast:285
assert_return(() => call($1, "eq", [-2147483648, 2147483647]), 0);

// i32.wast:286
assert_return(() => call($1, "eq", [2147483647, -2147483648]), 0);

// i32.wast:288
assert_return(() => call($1, "ne", [0, 0]), 0);

// i32.wast:289
assert_return(() => call($1, "ne", [1, 1]), 0);

// i32.wast:290
assert_return(() => call($1, "ne", [-1, 1]), 1);

// i32.wast:291
assert_return(() => call($1, "ne", [-2147483648, -2147483648]), 0);

// i32.wast:292
assert_return(() => call($1, "ne", [2147483647, 2147483647]), 0);

// i32.wast:293
assert_return(() => call($1, "ne", [-1, -1]), 0);

// i32.wast:294
assert_return(() => call($1, "ne", [1, 0]), 1);

// i32.wast:295
assert_return(() => call($1, "ne", [0, 1]), 1);

// i32.wast:296
assert_return(() => call($1, "ne", [-2147483648, 0]), 1);

// i32.wast:297
assert_return(() => call($1, "ne", [0, -2147483648]), 1);

// i32.wast:298
assert_return(() => call($1, "ne", [-2147483648, -1]), 1);

// i32.wast:299
assert_return(() => call($1, "ne", [-1, -2147483648]), 1);

// i32.wast:300
assert_return(() => call($1, "ne", [-2147483648, 2147483647]), 1);

// i32.wast:301
assert_return(() => call($1, "ne", [2147483647, -2147483648]), 1);

// i32.wast:303
assert_return(() => call($1, "lt_s", [0, 0]), 0);

// i32.wast:304
assert_return(() => call($1, "lt_s", [1, 1]), 0);

// i32.wast:305
assert_return(() => call($1, "lt_s", [-1, 1]), 1);

// i32.wast:306
assert_return(() => call($1, "lt_s", [-2147483648, -2147483648]), 0);

// i32.wast:307
assert_return(() => call($1, "lt_s", [2147483647, 2147483647]), 0);

// i32.wast:308
assert_return(() => call($1, "lt_s", [-1, -1]), 0);

// i32.wast:309
assert_return(() => call($1, "lt_s", [1, 0]), 0);

// i32.wast:310
assert_return(() => call($1, "lt_s", [0, 1]), 1);

// i32.wast:311
assert_return(() => call($1, "lt_s", [-2147483648, 0]), 1);

// i32.wast:312
assert_return(() => call($1, "lt_s", [0, -2147483648]), 0);

// i32.wast:313
assert_return(() => call($1, "lt_s", [-2147483648, -1]), 1);

// i32.wast:314
assert_return(() => call($1, "lt_s", [-1, -2147483648]), 0);

// i32.wast:315
assert_return(() => call($1, "lt_s", [-2147483648, 2147483647]), 1);

// i32.wast:316
assert_return(() => call($1, "lt_s", [2147483647, -2147483648]), 0);

// i32.wast:318
assert_return(() => call($1, "lt_u", [0, 0]), 0);

// i32.wast:319
assert_return(() => call($1, "lt_u", [1, 1]), 0);

// i32.wast:320
assert_return(() => call($1, "lt_u", [-1, 1]), 0);

// i32.wast:321
assert_return(() => call($1, "lt_u", [-2147483648, -2147483648]), 0);

// i32.wast:322
assert_return(() => call($1, "lt_u", [2147483647, 2147483647]), 0);

// i32.wast:323
assert_return(() => call($1, "lt_u", [-1, -1]), 0);

// i32.wast:324
assert_return(() => call($1, "lt_u", [1, 0]), 0);

// i32.wast:325
assert_return(() => call($1, "lt_u", [0, 1]), 1);

// i32.wast:326
assert_return(() => call($1, "lt_u", [-2147483648, 0]), 0);

// i32.wast:327
assert_return(() => call($1, "lt_u", [0, -2147483648]), 1);

// i32.wast:328
assert_return(() => call($1, "lt_u", [-2147483648, -1]), 1);

// i32.wast:329
assert_return(() => call($1, "lt_u", [-1, -2147483648]), 0);

// i32.wast:330
assert_return(() => call($1, "lt_u", [-2147483648, 2147483647]), 0);

// i32.wast:331
assert_return(() => call($1, "lt_u", [2147483647, -2147483648]), 1);

// i32.wast:333
assert_return(() => call($1, "le_s", [0, 0]), 1);

// i32.wast:334
assert_return(() => call($1, "le_s", [1, 1]), 1);

// i32.wast:335
assert_return(() => call($1, "le_s", [-1, 1]), 1);

// i32.wast:336
assert_return(() => call($1, "le_s", [-2147483648, -2147483648]), 1);

// i32.wast:337
assert_return(() => call($1, "le_s", [2147483647, 2147483647]), 1);

// i32.wast:338
assert_return(() => call($1, "le_s", [-1, -1]), 1);

// i32.wast:339
assert_return(() => call($1, "le_s", [1, 0]), 0);

// i32.wast:340
assert_return(() => call($1, "le_s", [0, 1]), 1);

// i32.wast:341
assert_return(() => call($1, "le_s", [-2147483648, 0]), 1);

// i32.wast:342
assert_return(() => call($1, "le_s", [0, -2147483648]), 0);

// i32.wast:343
assert_return(() => call($1, "le_s", [-2147483648, -1]), 1);

// i32.wast:344
assert_return(() => call($1, "le_s", [-1, -2147483648]), 0);

// i32.wast:345
assert_return(() => call($1, "le_s", [-2147483648, 2147483647]), 1);

// i32.wast:346
assert_return(() => call($1, "le_s", [2147483647, -2147483648]), 0);

// i32.wast:348
assert_return(() => call($1, "le_u", [0, 0]), 1);

// i32.wast:349
assert_return(() => call($1, "le_u", [1, 1]), 1);

// i32.wast:350
assert_return(() => call($1, "le_u", [-1, 1]), 0);

// i32.wast:351
assert_return(() => call($1, "le_u", [-2147483648, -2147483648]), 1);

// i32.wast:352
assert_return(() => call($1, "le_u", [2147483647, 2147483647]), 1);

// i32.wast:353
assert_return(() => call($1, "le_u", [-1, -1]), 1);

// i32.wast:354
assert_return(() => call($1, "le_u", [1, 0]), 0);

// i32.wast:355
assert_return(() => call($1, "le_u", [0, 1]), 1);

// i32.wast:356
assert_return(() => call($1, "le_u", [-2147483648, 0]), 0);

// i32.wast:357
assert_return(() => call($1, "le_u", [0, -2147483648]), 1);

// i32.wast:358
assert_return(() => call($1, "le_u", [-2147483648, -1]), 1);

// i32.wast:359
assert_return(() => call($1, "le_u", [-1, -2147483648]), 0);

// i32.wast:360
assert_return(() => call($1, "le_u", [-2147483648, 2147483647]), 0);

// i32.wast:361
assert_return(() => call($1, "le_u", [2147483647, -2147483648]), 1);

// i32.wast:363
assert_return(() => call($1, "gt_s", [0, 0]), 0);

// i32.wast:364
assert_return(() => call($1, "gt_s", [1, 1]), 0);

// i32.wast:365
assert_return(() => call($1, "gt_s", [-1, 1]), 0);

// i32.wast:366
assert_return(() => call($1, "gt_s", [-2147483648, -2147483648]), 0);

// i32.wast:367
assert_return(() => call($1, "gt_s", [2147483647, 2147483647]), 0);

// i32.wast:368
assert_return(() => call($1, "gt_s", [-1, -1]), 0);

// i32.wast:369
assert_return(() => call($1, "gt_s", [1, 0]), 1);

// i32.wast:370
assert_return(() => call($1, "gt_s", [0, 1]), 0);

// i32.wast:371
assert_return(() => call($1, "gt_s", [-2147483648, 0]), 0);

// i32.wast:372
assert_return(() => call($1, "gt_s", [0, -2147483648]), 1);

// i32.wast:373
assert_return(() => call($1, "gt_s", [-2147483648, -1]), 0);

// i32.wast:374
assert_return(() => call($1, "gt_s", [-1, -2147483648]), 1);

// i32.wast:375
assert_return(() => call($1, "gt_s", [-2147483648, 2147483647]), 0);

// i32.wast:376
assert_return(() => call($1, "gt_s", [2147483647, -2147483648]), 1);

// i32.wast:378
assert_return(() => call($1, "gt_u", [0, 0]), 0);

// i32.wast:379
assert_return(() => call($1, "gt_u", [1, 1]), 0);

// i32.wast:380
assert_return(() => call($1, "gt_u", [-1, 1]), 1);

// i32.wast:381
assert_return(() => call($1, "gt_u", [-2147483648, -2147483648]), 0);

// i32.wast:382
assert_return(() => call($1, "gt_u", [2147483647, 2147483647]), 0);

// i32.wast:383
assert_return(() => call($1, "gt_u", [-1, -1]), 0);

// i32.wast:384
assert_return(() => call($1, "gt_u", [1, 0]), 1);

// i32.wast:385
assert_return(() => call($1, "gt_u", [0, 1]), 0);

// i32.wast:386
assert_return(() => call($1, "gt_u", [-2147483648, 0]), 1);

// i32.wast:387
assert_return(() => call($1, "gt_u", [0, -2147483648]), 0);

// i32.wast:388
assert_return(() => call($1, "gt_u", [-2147483648, -1]), 0);

// i32.wast:389
assert_return(() => call($1, "gt_u", [-1, -2147483648]), 1);

// i32.wast:390
assert_return(() => call($1, "gt_u", [-2147483648, 2147483647]), 1);

// i32.wast:391
assert_return(() => call($1, "gt_u", [2147483647, -2147483648]), 0);

// i32.wast:393
assert_return(() => call($1, "ge_s", [0, 0]), 1);

// i32.wast:394
assert_return(() => call($1, "ge_s", [1, 1]), 1);

// i32.wast:395
assert_return(() => call($1, "ge_s", [-1, 1]), 0);

// i32.wast:396
assert_return(() => call($1, "ge_s", [-2147483648, -2147483648]), 1);

// i32.wast:397
assert_return(() => call($1, "ge_s", [2147483647, 2147483647]), 1);

// i32.wast:398
assert_return(() => call($1, "ge_s", [-1, -1]), 1);

// i32.wast:399
assert_return(() => call($1, "ge_s", [1, 0]), 1);

// i32.wast:400
assert_return(() => call($1, "ge_s", [0, 1]), 0);

// i32.wast:401
assert_return(() => call($1, "ge_s", [-2147483648, 0]), 0);

// i32.wast:402
assert_return(() => call($1, "ge_s", [0, -2147483648]), 1);

// i32.wast:403
assert_return(() => call($1, "ge_s", [-2147483648, -1]), 0);

// i32.wast:404
assert_return(() => call($1, "ge_s", [-1, -2147483648]), 1);

// i32.wast:405
assert_return(() => call($1, "ge_s", [-2147483648, 2147483647]), 0);

// i32.wast:406
assert_return(() => call($1, "ge_s", [2147483647, -2147483648]), 1);

// i32.wast:408
assert_return(() => call($1, "ge_u", [0, 0]), 1);

// i32.wast:409
assert_return(() => call($1, "ge_u", [1, 1]), 1);

// i32.wast:410
assert_return(() => call($1, "ge_u", [-1, 1]), 1);

// i32.wast:411
assert_return(() => call($1, "ge_u", [-2147483648, -2147483648]), 1);

// i32.wast:412
assert_return(() => call($1, "ge_u", [2147483647, 2147483647]), 1);

// i32.wast:413
assert_return(() => call($1, "ge_u", [-1, -1]), 1);

// i32.wast:414
assert_return(() => call($1, "ge_u", [1, 0]), 1);

// i32.wast:415
assert_return(() => call($1, "ge_u", [0, 1]), 0);

// i32.wast:416
assert_return(() => call($1, "ge_u", [-2147483648, 0]), 1);

// i32.wast:417
assert_return(() => call($1, "ge_u", [0, -2147483648]), 0);

// i32.wast:418
assert_return(() => call($1, "ge_u", [-2147483648, -1]), 0);

// i32.wast:419
assert_return(() => call($1, "ge_u", [-1, -2147483648]), 1);

// i32.wast:420
assert_return(() => call($1, "ge_u", [-2147483648, 2147483647]), 1);

// i32.wast:421
assert_return(() => call($1, "ge_u", [2147483647, -2147483648]), 0);
