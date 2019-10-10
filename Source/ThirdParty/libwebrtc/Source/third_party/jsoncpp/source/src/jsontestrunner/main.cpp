// Copyright 2007-2010 Baptiste Lepilleur and The JsonCpp Authors
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif

/* This executable is used for testing parser/writer using real JSON files.
 */

#include <algorithm> // sort
#include <cstdio>
#include <json/json.h>
#include <sstream>

struct Options {
  Json::String path;
  Json::Features features;
  bool parseOnly;
  using writeFuncType = Json::String (*)(Json::Value const&);
  writeFuncType write;
};

static Json::String normalizeFloatingPointStr(double value) {
  char buffer[32];
  jsoncpp_snprintf(buffer, sizeof(buffer), "%.16g", value);
  buffer[sizeof(buffer) - 1] = 0;
  Json::String s(buffer);
  Json::String::size_type index = s.find_last_of("eE");
  if (index != Json::String::npos) {
    Json::String::size_type hasSign =
        (s[index + 1] == '+' || s[index + 1] == '-') ? 1 : 0;
    Json::String::size_type exponentStartIndex = index + 1 + hasSign;
    Json::String normalized = s.substr(0, exponentStartIndex);
    Json::String::size_type indexDigit =
        s.find_first_not_of('0', exponentStartIndex);
    Json::String exponent = "0";
    if (indexDigit != Json::String::npos) // There is an exponent different
                                          // from 0
    {
      exponent = s.substr(indexDigit);
    }
    return normalized + exponent;
  }
  return s;
}

static Json::String readInputTestFile(const char* path) {
  FILE* file = fopen(path, "rb");
  if (!file)
    return "";
  fseek(file, 0, SEEK_END);
  long const size = ftell(file);
  size_t const usize = static_cast<unsigned long>(size);
  fseek(file, 0, SEEK_SET);
  char* buffer = new char[size + 1];
  buffer[size] = 0;
  Json::String text;
  if (fread(buffer, 1, usize, file) == usize)
    text = buffer;
  fclose(file);
  delete[] buffer;
  return text;
}

static void
printValueTree(FILE* fout, Json::Value& value, const Json::String& path = ".") {
  if (value.hasComment(Json::commentBefore)) {
    fprintf(fout, "%s\n", value.getComment(Json::commentBefore).c_str());
  }
  switch (value.type()) {
  case Json::nullValue:
    fprintf(fout, "%s=null\n", path.c_str());
    break;
  case Json::intValue:
    fprintf(fout, "%s=%s\n", path.c_str(),
            Json::valueToString(value.asLargestInt()).c_str());
    break;
  case Json::uintValue:
    fprintf(fout, "%s=%s\n", path.c_str(),
            Json::valueToString(value.asLargestUInt()).c_str());
    break;
  case Json::realValue:
    fprintf(fout, "%s=%s\n", path.c_str(),
            normalizeFloatingPointStr(value.asDouble()).c_str());
    break;
  case Json::stringValue:
    fprintf(fout, "%s=\"%s\"\n", path.c_str(), value.asString().c_str());
    break;
  case Json::booleanValue:
    fprintf(fout, "%s=%s\n", path.c_str(), value.asBool() ? "true" : "false");
    break;
  case Json::arrayValue: {
    fprintf(fout, "%s=[]\n", path.c_str());
    Json::ArrayIndex size = value.size();
    for (Json::ArrayIndex index = 0; index < size; ++index) {
      static char buffer[16];
      jsoncpp_snprintf(buffer, sizeof(buffer), "[%u]", index);
      printValueTree(fout, value[index], path + buffer);
    }
  } break;
  case Json::objectValue: {
    fprintf(fout, "%s={}\n", path.c_str());
    Json::Value::Members members(value.getMemberNames());
    std::sort(members.begin(), members.end());
    Json::String suffix = *(path.end() - 1) == '.' ? "" : ".";
    for (auto name : members) {
      printValueTree(fout, value[name], path + suffix + name);
    }
  } break;
  default:
    break;
  }

  if (value.hasComment(Json::commentAfter)) {
    fprintf(fout, "%s\n", value.getComment(Json::commentAfter).c_str());
  }
}

static int parseAndSaveValueTree(const Json::String& input,
                                 const Json::String& actual,
                                 const Json::String& kind,
                                 const Json::Features& features,
                                 bool parseOnly,
                                 Json::Value* root) {
  Json::Reader reader(features);
  bool parsingSuccessful =
      reader.parse(input.data(), input.data() + input.size(), *root);
  if (!parsingSuccessful) {
    printf("Failed to parse %s file: \n%s\n", kind.c_str(),
           reader.getFormattedErrorMessages().c_str());
    return 1;
  }
  if (!parseOnly) {
    FILE* factual = fopen(actual.c_str(), "wt");
    if (!factual) {
      printf("Failed to create %s actual file.\n", kind.c_str());
      return 2;
    }
    printValueTree(factual, *root);
    fclose(factual);
  }
  return 0;
}
// static Json::String useFastWriter(Json::Value const& root) {
//   Json::FastWriter writer;
//   writer.enableYAMLCompatibility();
//   return writer.write(root);
// }
static Json::String useStyledWriter(Json::Value const& root) {
  Json::StyledWriter writer;
  return writer.write(root);
}
static Json::String useStyledStreamWriter(Json::Value const& root) {
  Json::StyledStreamWriter writer;
  Json::OStringStream sout;
  writer.write(sout, root);
  return sout.str();
}
static Json::String useBuiltStyledStreamWriter(Json::Value const& root) {
  Json::StreamWriterBuilder builder;
  return Json::writeString(builder, root);
}
static int rewriteValueTree(const Json::String& rewritePath,
                            const Json::Value& root,
                            Options::writeFuncType write,
                            Json::String* rewrite) {
  *rewrite = write(root);
  FILE* fout = fopen(rewritePath.c_str(), "wt");
  if (!fout) {
    printf("Failed to create rewrite file: %s\n", rewritePath.c_str());
    return 2;
  }
  fprintf(fout, "%s\n", rewrite->c_str());
  fclose(fout);
  return 0;
}

static Json::String removeSuffix(const Json::String& path,
                                 const Json::String& extension) {
  if (extension.length() >= path.length())
    return Json::String("");
  Json::String suffix = path.substr(path.length() - extension.length());
  if (suffix != extension)
    return Json::String("");
  return path.substr(0, path.length() - extension.length());
}

static void printConfig() {
// Print the configuration used to compile JsonCpp
#if defined(JSON_NO_INT64)
  printf("JSON_NO_INT64=1\n");
#else
  printf("JSON_NO_INT64=0\n");
#endif
}

static int printUsage(const char* argv[]) {
  printf("Usage: %s [--strict] input-json-file", argv[0]);
  return 3;
}

static int parseCommandLine(int argc, const char* argv[], Options* opts) {
  opts->parseOnly = false;
  opts->write = &useStyledWriter;
  if (argc < 2) {
    return printUsage(argv);
  }
  int index = 1;
  if (Json::String(argv[index]) == "--json-checker") {
    opts->features = Json::Features::strictMode();
    opts->parseOnly = true;
    ++index;
  }
  if (Json::String(argv[index]) == "--json-config") {
    printConfig();
    return 3;
  }
  if (Json::String(argv[index]) == "--json-writer") {
    ++index;
    Json::String const writerName(argv[index++]);
    if (writerName == "StyledWriter") {
      opts->write = &useStyledWriter;
    } else if (writerName == "StyledStreamWriter") {
      opts->write = &useStyledStreamWriter;
    } else if (writerName == "BuiltStyledStreamWriter") {
      opts->write = &useBuiltStyledStreamWriter;
    } else {
      printf("Unknown '--json-writer %s'\n", writerName.c_str());
      return 4;
    }
  }
  if (index == argc || index + 1 < argc) {
    return printUsage(argv);
  }
  opts->path = argv[index];
  return 0;
}
static int runTest(Options const& opts) {
  int exitCode = 0;

  Json::String input = readInputTestFile(opts.path.c_str());
  if (input.empty()) {
    printf("Failed to read input or empty input: %s\n", opts.path.c_str());
    return 3;
  }

  Json::String basePath = removeSuffix(opts.path, ".json");
  if (!opts.parseOnly && basePath.empty()) {
    printf("Bad input path. Path does not end with '.expected':\n%s\n",
           opts.path.c_str());
    return 3;
  }

  Json::String const actualPath = basePath + ".actual";
  Json::String const rewritePath = basePath + ".rewrite";
  Json::String const rewriteActualPath = basePath + ".actual-rewrite";

  Json::Value root;
  exitCode = parseAndSaveValueTree(input, actualPath, "input", opts.features,
                                   opts.parseOnly, &root);
  if (exitCode || opts.parseOnly) {
    return exitCode;
  }
  Json::String rewrite;
  exitCode = rewriteValueTree(rewritePath, root, opts.write, &rewrite);
  if (exitCode) {
    return exitCode;
  }
  Json::Value rewriteRoot;
  exitCode = parseAndSaveValueTree(rewrite, rewriteActualPath, "rewrite",
                                   opts.features, opts.parseOnly, &rewriteRoot);
  if (exitCode) {
    return exitCode;
  }
  return 0;
}
int main(int argc, const char* argv[]) {
  Options opts;
  try {
    int exitCode = parseCommandLine(argc, argv, &opts);
    if (exitCode != 0) {
      printf("Failed to parse command-line.");
      return exitCode;
    }
    return runTest(opts);
  } catch (const std::exception& e) {
    printf("Unhandled exception:\n%s\n", e.what());
    return 1;
  }
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
