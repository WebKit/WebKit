# WebM Parser {#mainpage}

# Introduction

This WebM parser is a C++11-based parser that aims to be a safe and complete
parser for WebM. It supports all WebM elements (from the old deprecated ones to
the newest ones like `Colour`), including recursive elements like `ChapterAtom`
and `SimpleTag`. It supports incremental parsing; parsing may be stopped at any
point and resumed later as needed. It also supports starting at an arbitrary
WebM element, so parsing need not start from the beginning of the file.

The parser (`WebmParser`) works by being fed input data from a data source (an
instance of `Reader`) that represents a WebM file. The parser will parse the
WebM data into various data structures that represent the encoded WebM elements,
and then call corresponding `Callback` event methods as the data structures are
parsed.

# Building

CMake support has been added to the root libwebm `CMakeLists.txt` file. Simply
enable the `ENABLE_WEBM_PARSER` feature if using the interactive CMake builder,
or alternatively pass the `-DENABLE_WEBM_PARSER:BOOL=ON` flag from the command
line. By default, this parser is not enabled when building libwebm, so you must
explicitly enable it.

Alternatively, the following illustrates the minimal commands necessary to
compile the code into a static library without CMake:

```.sh
c++ -Iinclude -I. -std=c++11 -c src/*.cc
ar rcs libwebm.a *.o
```

# Using the parser

There are 3 basic components in the parser that are used: `Reader`, `Callback`,
and `WebmParser`.

## `Reader`

The `Reader` interface acts as a data source for the parser. You may subclass it
and implement your own data source if you wish. Alternatively, use the
`FileReader`, `IstreamReader`, or `BufferReader` if you wish to read from a
`FILE*`, `std::istream`, or `std::vector<std::uint8_t>`, respectively.

The parser supports `Reader` implementations that do short reads. If
`Reader::Skip()` or `Reader::Read()` do a partial read (returning
`Status::kOkPartial`), the parser will call them again in an attempt to read
more data. If no data is available, the `Reader` may return some other status
(like `Status::kWouldBlock`) to indicate that no data is available. In this
situation, the parser will stop parsing and return the status it received.
Parsing may be resumed later when more data is available.

When the `Reader` has reached the end of the WebM document and no more data is
available, it should return `Status::kEndOfFile`. This will cause parsing to
stop. If the file ends at a valid location (that is, there aren't any elements
that have specified a size that indicates the file ended prematurely), the
parser will translate `Status::kEndOfFile` into `Status::kOkCompleted` and
return it. If the file ends prematurely, the parser will return
`Status::kEndOfFile` to indicate that.

Note that if the WebM file contains elements that have an unknown size (or a
seek has been performed and the parser doesn't know the size of the root
element(s)), and the parser is parsing them and hits end-of-file, the parser may
still call `Reader::Read()`/`Reader::Skip()` multiple times (even though they've
already reported `Status::kEndOfFile`) as nested parsers terminate parsing.
Because of this, `Reader::Read()`/`Reader::Skip()` implementations should be
able to handle being called multiple times after the file's end has been
reached, and they should consistently return `Status::kEndOfFile`.

The three provided readers (`FileReader`, `IstreamReader`, and `BufferReader`)
are blocking implementations (they won't return `Status::kWouldBlock`), so if
you're using them the parser will run until it entirely consumes all their data
(unless, of course, you request the parser to stop via `Callback`... see the
next section).

## `Callback`

As the parser progresses through the file, it builds objects (see
`webm/dom_types.h`) that represent parsed data structures. The parser then
notifies the `Callback` implementation as objects complete parsing. For some
data structures (like frames or Void elements), the parser notifies the
`Callback` and requests it to consume the data directly from the `Reader` (this
is done for structures that can be large/frequent binary blobs in order to allow
you to read the data directly into the object/type of your choice, rather than
just reading them into a `std::vector<std::uint8_t>` and making you copy it into
a different object if you wanted to work with something other than
`std::vector<std::uint8_t>`).

The parser was designed to parse the data into objects that are small enough
that the `Callback` can be quickly and frequently notified as soon as the object
is ready, but large enough that the objects received by the `Callback` are still
useful. Having `Callback` events for every tiny integer/float/string/etc.
element would require too much assembly and work to be useful to most users, and
pasing the file into a single DOM tree (or a small handful of large conglomerate
structures) would unnecessarily delay video playback or consume too much memory
on smaller devices.

The parser may call the following methods while nearly anywhere in the file:

-   `Callback::OnElementBegin()`: This is called for every element that the
    parser encounters. This is primarily useful if you want to skip some
    elements or build a map of every element in the file.
-   `Callback::OnUnknownElement()`: This is called when an element is either not
    a valid/recognized WebM element, or it is a WebM element but is improperly
    nested (e.g. an EBMLVersion element inside of a Segment element). The parser
    doesn't know how to handle the element; it could just skip it but instead
    defers to the `Callback` to decide how it should be handled. The default
    implementation just skips the element.
-   `Callback::OnVoid()`: Void elements can appear anywhere in any master
    element. This method will be called to handle the Void element.

The parser may call the following methods in the proper nesting order, as shown
in the list. A `*Begin()` method will always be matched up with its
corresponding `*End()` method (unless a seek has been performed). The parser
will only call the methods in the proper nesting order as specified in the WebM
DOM. For example, `Callback::OnEbml()` will never be called in between
`Callback::OnSegmentBegin()`/`Callback::OnSegmentEnd()` (since the EBML element
is not a child of the Segment element), and `Callback::OnTrackEntry()` will only
ever be called in between
`Callback::OnSegmentBegin()`/`Callback::OnSegmentEnd()` (since the TrackEntry
element is a (grand-)child of the Segment element and must be contained by a
Segment element). `Callback::OnFrame()` is listed twice because it will be
called to handle frames contained in both SimpleBlock and Block elements.

-   `Callback::OnEbml()`
-   `Callback::OnSegmentBegin()`
    -   `Callback::OnSeek()`
    -   `Callback::OnInfo()`
    -   `Callback::OnClusterBegin()`
        -   `Callback::OnSimpleBlockBegin()`
            -   `Callback::OnFrame()`
        -   `Callback::OnSimpleBlockEnd()`
        -   `Callback::OnBlockGroupBegin()`
            -   `Callback::OnBlockBegin()`
                -   `Callback::OnFrame()`
            -   `Callback::OnBlockEnd()`
        -   `Callback::OnBlockGroupEnd()`
    -   `Callback::OnClusterEnd()`
    -   `Callback::OnTrackEntry()`
    -   `Callback::OnCuePoint()`
    -   `Callback::OnEditionEntry()`
    -   `Callback::OnTag()`
-   `Callback::OnSegmentEnd()`

Only `Callback::OnFrame()` (and no other `Callback` methods) will be called in
between `Callback::OnSimpleBlockBegin()`/`Callback::OnSimpleBlockEnd()` or
`Callback::OnBlockBegin()`/`Callback::OnBlockEnd()`, since the SimpleBlock and
Block elements are not master elements only contain frames.

Note that seeking into the middle of the file may cause the parser to skip some
`*Begin()` methods. For example, if a seek is performed to a SimpleBlock
element, `Callback::OnSegmentBegin()` and `Callback::OnClusterBegin()` will not
be called. In this situation, the full sequence of callback events would be
(assuming the file ended after the SimpleBlock):
`Callback::OnSimpleBlockBegin()`, `Callback::OnFrame()` (for every frame in the
SimpleBlock), `Callback::OnSimpleBlockEnd()`, `Callback::OnClusterEnd()`, and
`Callback::OnSegmentEnd()`. Since the Cluster and Segment elements were skipped,
the `Cluster` DOM object may have some members marked as absent, and the
`*End()` events for the Cluster and Segment elements will have metadata with
unknown header position, header length, and body size (see `kUnknownHeaderSize`,
`kUnknownElementSize`, and `kUnknownElementPosition`).

When a `Callback` method has completed, it should return `Status::kOkCompleted`
to allow parsing to continue. If you would like parsing to stop, return any
other status code (except `Status::kEndOfFile`, since that's treated somewhat
specially and is intended for `Reader`s to use), which the parser will return.
If you return a non-parsing-error status code (.e.g. `Status::kOkPartial`,
`Status::kWouldBlock`, etc. or your own status code with a value > 0), parsing
may be resumed again. When parsing is resumed, the parser will call the same
callback method again (and once again, you may return `Status::kOkCompleted` to
let parsing continue or some other value to stop parsing).

You may subclass the `Callback` element and override methods which you are
interested in receiving events for. By default, methods taking an `Action`
parameter will set it to `Action::kRead` so the entire file is parsed. The
`Callback::OnFrame()` method will just skip over the frame bytes by default.

## `WebmParser`

The actual parsing work is done with `WebmParser`. Simply construct a
`WebmParser` and call `WebmParser::Feed()` (providing it a `Callback` and
`Reader` instance) to parse a file. It will return `Status::kOkCompleted` when
the entire file has been successfully parsed. `WebmParser::Feed()` doesn't store
any internal references to the `Callback` or `Reader`.

If you wish to start parsing from the middle of a file, call
`WebmParser::DidSeek()` before calling `WebmParser::Feed()` to prepare the
parser to receive data starting at an arbitrary point in the file. When seeking,
you should seek to the beginning of a WebM element; seeking to a location that
is not the start of a WebM element (e.g. seeking to a frame, rather than its
containing SimpleBlock/Block element) will cause parsing to fail. Calling
`WebmParser::DidSeek()` will reset the state of the parser and clear any
internal errors, so a `WebmParser` instance may be reused (even if it has
previously failed to parse a file).

## Building your program

The following program is a small program that completely parses a file from
stdin:

```.cc
#include <webm/callback.h>
#include <webm/file_reader.h>
#include <webm/webm_parser.h>

int main() {
  webm::Callback callback;
  webm::FileReader reader(std::freopen(nullptr, "rb", stdin));
  webm::WebmParser parser;
  parser.Feed(&callback, &reader);
}
```

It completely parses the input file, but we need to make a new class that
derives from `Callback` if we want to receive any parsing events. So if we
change it to:

```.cc
#include <iomanip>
#include <iostream>

#include <webm/callback.h>
#include <webm/file_reader.h>
#include <webm/status.h>
#include <webm/webm_parser.h>

class MyCallback : public webm::Callback {
 public:
  webm::Status OnElementBegin(const webm::ElementMetadata& metadata,
                              webm::Action* action) override {
    std::cout << "Element ID = 0x"
              << std::hex << static_cast<std::uint32_t>(metadata.id);
    std::cout << std::dec;  // Reset to decimal mode.
    std::cout << " at position ";
    if (metadata.position == webm::kUnknownElementPosition) {
      // The position will only be unknown if we've done a seek. But since we
      // aren't seeking in this demo, this will never be the case. However, this
      // if-statement is included for completeness.
      std::cout << "<unknown>";
    } else {
      std::cout << metadata.position;
    }
    std::cout << " with header size ";
    if (metadata.header_size == webm::kUnknownHeaderSize) {
      // The header size will only be unknown if we've done a seek. But since we
      // aren't seeking in this demo, this will never be the case. However, this
      // if-statement is included for completeness.
      std::cout << "<unknown>";
    } else {
      std::cout << metadata.header_size;
    }
    std::cout << " and body size ";
    if (metadata.size == webm::kUnknownElementSize) {
      // WebM master elements may have an unknown size, though this is rare.
      std::cout << "<unknown>";
    } else {
      std::cout << metadata.size;
    }
    std::cout << '\n';

    *action = webm::Action::kRead;
    return webm::Status(webm::Status::kOkCompleted);
  }
};

int main() {
  MyCallback callback;
  webm::FileReader reader(std::freopen(nullptr, "rb", stdin));
  webm::WebmParser parser;
  webm::Status status = parser.Feed(&callback, &reader);
  if (status.completed_ok()) {
    std::cout << "Parsing successfully completed\n";
  } else {
    std::cout << "Parsing failed with status code: " << status.code << '\n';
  }
}
```

This will output information about every element in the entire file: it's ID,
position, header size, and body size. The status of the parse is also checked
and reported.

For a more complete example, see `demo/demo.cc`, which parses an entire file and
prints out all of its information. That example overrides every `Callback`
method to show exactly what information is available while parsing and how to
access it. The example is verbose, but that's primarily due to pretty-printing
and string formatting operations.

When compiling your program, add the `include` directory to your compiler's
header search paths and link to the compiled library. Be sure your compiler has
C++11 mode enabled (`-std=c++11` in clang++ or g++).

# Testing

Unit tests are located in the `tests` directory. Google Test and Google Mock are
used as testing frameworks. Building and running the tests will be supported in
the upcoming CMake scripts, but they can currently be built and run by manually
compiling them (and linking to Google Test and Google Mock).

# Fuzzing

The parser has been fuzzed with [AFL](http://lcamtuf.coredump.cx/afl/) and
[libFuzzer](http://llvm.org/docs/LibFuzzer.html). If you wish to fuzz the parser
with AFL or libFuzzer but don't want to write an executable that exercises the
parsing API, you may use `fuzzing/webm_fuzzer.cc`.

When compiling for fuzzing, define the macro
`WEBM_FUZZER_BYTE_ELEMENT_SIZE_LIMIT` to be some integer in order to limit the
maximum size of ASCII/UTF-8/binary elements. It's too easy for the fuzzer to
generate elements that claim to have a ridiculously massive size, which will
cause allocations to fail or the program to allocate too much memory. AFL will
terminate the process if it allocates too much memory (by default, 50 MB), and
the [Address Sanitizer doesn't throw `std::bad_alloc` when an allocation fails]
(https://github.com/google/sanitizers/issues/295). Defining
`WEBM_FUZZER_BYTE_ELEMENT_SIZE_LIMIT` to a low number (say, 1024) will cause the
ASCII/UTF-8/binary element parsers to return `Status::kNotEnoughMemory` if the
element's size exceeds `WEBM_FUZZER_BYTE_ELEMENT_SIZE_LIMIT`, which will avoid
false positives when fuzzing. The parser expects `std::string` and `std::vector`
to throw `std::bad_alloc` when an allocation fails, which doesn't necessarily
happen due to the fuzzers' limitations.

You may also define the macro `WEBM_FUZZER_SEEK_FIRST` to have
`fuzzing/webm_fuzzer.cc` call `WebmParser::DidSeek()` before doing any parsing.
This will test the seeking code paths.
