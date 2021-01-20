# JsonCpp

[![badge](https://img.shields.io/badge/conan.io-jsoncpp%2F1.8.0-green.svg?logo=data:image/png;base64%2CiVBORw0KGgoAAAANSUhEUgAAAA4AAAAOCAMAAAAolt3jAAAA1VBMVEUAAABhlctjlstkl8tlmMtlmMxlmcxmmcxnmsxpnMxpnM1qnc1sn85voM91oM11oc1xotB2oc56pNF6pNJ2ptJ8ptJ8ptN9ptN8p9N5qNJ9p9N9p9R8qtOBqdSAqtOAqtR%2BrNSCrNJ/rdWDrNWCsNWCsNaJs9eLs9iRvNuVvdyVv9yXwd2Zwt6axN6dxt%2Bfx%2BChyeGiyuGjyuCjyuGly%2BGlzOKmzOGozuKoz%2BKqz%2BOq0OOv1OWw1OWw1eWx1eWy1uay1%2Baz1%2Baz1%2Bez2Oe02Oe12ee22ujUGwH3AAAAAXRSTlMAQObYZgAAAAFiS0dEAIgFHUgAAAAJcEhZcwAACxMAAAsTAQCanBgAAAAHdElNRQfgBQkREyOxFIh/AAAAiklEQVQI12NgAAMbOwY4sLZ2NtQ1coVKWNvoc/Eq8XDr2wB5Ig62ekza9vaOqpK2TpoMzOxaFtwqZua2Bm4makIM7OzMAjoaCqYuxooSUqJALjs7o4yVpbowvzSUy87KqSwmxQfnsrPISyFzWeWAXCkpMaBVIC4bmCsOdgiUKwh3JojLgAQ4ZCE0AMm2D29tZwe6AAAAAElFTkSuQmCC)](https://bintray.com/theirix/conan-repo/jsoncpp%3Atheirix)

[JSON][json-org] is a lightweight data-interchange format. It can represent
numbers, strings, ordered sequences of values, and collections of name/value
pairs.

[json-org]: http://json.org/

JsonCpp is a C++ library that allows manipulating JSON values, including
serialization and deserialization to and from strings. It can also preserve
existing comment in unserialization/serialization steps, making it a convenient
format to store user input files.


## Documentation

[JsonCpp documentation][JsonCpp-documentation] is generated using [Doxygen][].

[JsonCpp-documentation]: http://open-source-parsers.github.io/jsoncpp-docs/doxygen/index.html
[Doxygen]: http://www.doxygen.org


## A note on backward-compatibility

* `1.y.z` is built with C++11.
* `0.y.z` can be used with older compilers.
* Major versions maintain binary-compatibility.


## Using JsonCpp in your project

### Amalgamated source
https://github.com/open-source-parsers/jsoncpp/wiki/Amalgamated

### The Meson Build System
If you are using the [Meson Build System](http://mesonbuild.com), then you can get a wrap file by downloading it from [Meson WrapDB](https://wrapdb.mesonbuild.com/jsoncpp), or simply use `meson wrap install jsoncpp`.

### Other ways
If you have trouble, see the Wiki, or post a question as an Issue.

## License

See the `LICENSE` file for details. In summary, JsonCpp is licensed under the
MIT license, or public domain if desired and recognized in your jurisdiction.
