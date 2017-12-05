function __f_5484(__v_22596) {
  if (!__v_22596) throw new Error();
}

try {
  noInline(__f_5484);
} catch (e) {}

function __f_5485(...__v_22597) {
  return __v_22597[-13];
}

try {
  noInline(__f_5485);
} catch (e) {}

for (let __v_22598 = 0; __v_22598 < 10000; __v_22598++) {
  try {
    __f_5484(__f_5485(__v_22598) === __v_22598);
  } catch (e) {}
}
