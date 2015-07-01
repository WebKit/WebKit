deps = {
  "third_party/gyp":
      "http://gyp.googlecode.com/svn/trunk@1987",

  # TODO(kbr): figure out how to better stay in sync with Chromium's
  # versions of googletest and googlemock.
  "src/tests/third_party/googletest":
      "http://googletest.googlecode.com/svn/trunk@629",

  "src/tests/third_party/googlemock":
      "http://googlemock.googlecode.com/svn/trunk@410",

  "src/tests/third_party/deqp":
      "https://android.googlesource.com/platform/external/deqp@c7661bcd3bcec04b1abf6c3b290c4150db565604",

  "src/tests/third_party/libpng":
      "https://android.googlesource.com/platform/external/libpng@094e181e79a3d6c23fd005679025058b7df1ad6c",

  "src/tests/third_party/zlib":
      "https://chromium.googlesource.com/chromium/src/third_party/zlib@afd8c4593c010c045902f6c0501718f1823064a3",
}

hooks = [
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "build/gyp_angle"],
  },
]
