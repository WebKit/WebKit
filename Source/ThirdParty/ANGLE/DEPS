deps = {
  "third_party/gyp":
      "http://gyp.googlecode.com/svn/trunk@1806",

  "tests/third_party/googletest":
      "http://googletest.googlecode.com/svn/trunk@629",

  "tests/third_party/googlemock":
      "http://googlemock.googlecode.com/svn/trunk@410",
}

hooks = [
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "build/gyp_angle"],
  },
]
