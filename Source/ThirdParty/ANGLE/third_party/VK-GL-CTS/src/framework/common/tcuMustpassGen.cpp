/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2026 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Mustpass file generation runmode (--deqp-runmode=gen-mustpass).
 *//*--------------------------------------------------------------------*/

#include "tcuMustpassGen.hpp"

#include "tcuCommandLine.hpp"
#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestHierarchyIterator.hpp"

#include "deFilePath.hpp"
#include "qpDebugOut.h"

#include "json/json.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tcu
{

namespace
{

// PatternList
//
// Filter patterns are bucketed by shape: literal names go into a hash set,
// trailing-'*' prefixes into a small list, mid-string globs into a list with
// a literal-head gate before the wildcard matcher runs.

class PatternList
{
public:
    void add(const std::string &pattern)
    {
        const std::size_t starCount = std::count(pattern.begin(), pattern.end(), '*');
        if (starCount == 0)
        {
            m_named.insert(pattern);
        }
        else if (starCount == 1 && !pattern.empty() && pattern.back() == '*')
        {
            m_prefixes.push_back(pattern.substr(0, pattern.size() - 1));
        }
        else
        {
            const std::size_t firstStar = pattern.find('*');
            m_complex.emplace_back(pattern.substr(0, firstStar), pattern);
        }
    }

    // Match `caseName` against any pattern in this list.
    bool match(const std::string &caseName) const
    {
        // Named: exact-string membership in the hash set is O(1).
        if (m_named.find(caseName) != m_named.end())
            return true;
        // Prefix-only: walk the small list and check startswith.
        for (const std::string &prefix : m_prefixes)
        {
            if (caseName.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), caseName.begin()))
                return true;
        }
        // Complex: literal-prefix gate, then full glob via matchWildcards.
        for (const auto &kv : m_complex)
        {
            const std::string &literalPrefix = kv.first;
            if (caseName.size() < literalPrefix.size() ||
                !std::equal(literalPrefix.begin(), literalPrefix.end(), caseName.begin()))
                continue;
            const std::string &p = kv.second;
            if (matchWildcards(p.begin(), p.end(), caseName.begin(), caseName.end(), false))
                return true;
        }
        return false;
    }

private:
    std::unordered_set<std::string> m_named;
    std::vector<std::string> m_prefixes;                        // literal prefix (pattern minus trailing '*')
    std::vector<std::pair<std::string, std::string>> m_complex; // (literal-head, original-pattern)
};

struct Filter
{
    enum Type
    {
        TYPE_INCLUDE,
        TYPE_EXCLUDE
    };
    Type type = TYPE_INCLUDE;
    PatternList patterns;
};

// Streaming sink for one mustpass output file.  Opened on first append.
struct OutputSink
{
    std::ofstream out;
};

struct Config
{
    std::string name;
    std::string outputFile;  // relative to outputBaseDir
    std::string groupSubDir; // relative to outputBaseDir; empty if no split
    std::vector<std::string> splitGroups;
    std::vector<Filter> filters;
    // Sinks keyed by absolute output path.  outputOrder records insertion
    // (= first-touch) order.  Tree walk visits cases alphabetically, so the
    // first sub-file touched is also the one with the smallest test name --
    // exactly the order the split-group index needs.  No sort required.
    std::unordered_map<std::string, OutputSink> outputs;
    std::vector<std::string> outputOrder;

    Config()                     = default;
    Config(Config &&)            = default;
    Config &operator=(Config &&) = default;
};

struct Spec
{
    std::string outputBaseDir;
    std::vector<Config> configs;
};

// Spec parsing
//
// JSON spec produced by scripts/mustpass.py:emitMustpassSpec().  Example:
//   {
//     "output_base_dir": "/abs/path/to/external/vulkancts/mustpass/main",
//     "configs": [
//       {
//         "name": "default",
//         "output_file": "vk-default.txt",
//         "group_subdir": "vk-default",
//         "split_groups": ["dEQP-VK", "dEQP-VK.pipeline"],
//         "filters": [
//           {"type": "include", "files": ["/path/to/main.txt"]},
//           {"type": "exclude", "files": ["/path/to/test-issues.txt",
//                                         "/path/to/excluded-tests.txt"]}
//         ]
//       }
//     ]
//   }
//
// Patterns within a single filter OR together; filters AND together.

void loadPatternsFromFile(const std::string &path, PatternList &dst)
{
    std::ifstream f(path.c_str());
    if (!f.is_open())
        throw Exception("Failed to open pattern file: " + path);
    std::string line;
    while (std::getline(f, line))
    {
        std::size_t b = 0;
        std::size_t e = line.size();
        while (b < e && (line[b] == ' ' || line[b] == '\t' || line[b] == '\r'))
            ++b;
        while (e > b && (line[e - 1] == ' ' || line[e - 1] == '\t' || line[e - 1] == '\r'))
            --e;
        const std::string t = line.substr(b, e - b);
        if (t.empty() || t[0] == '#')
            continue;
        dst.add(t);
    }
}

void requireMember(const Json::Value &v, const char *key, const std::string &where)
{
    if (!v.isMember(key))
        throw Exception(std::string("Mustpass spec ") + where + ": missing required field '" + key + "'");
}

std::string requireString(const Json::Value &v, const char *key, const std::string &where)
{
    requireMember(v, key, where);
    const Json::Value &m = v[key];
    if (!m.isString())
        throw Exception(std::string("Mustpass spec ") + where + ": field '" + key + "' must be a string");
    return m.asString();
}

Spec parseSpec(const std::string &specPath)
{
    std::ifstream f(specPath.c_str());
    if (!f.is_open())
        throw Exception("Failed to open mustpass spec: " + specPath);

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, f, &root, &errs))
        throw Exception("Failed to parse mustpass spec " + specPath + ": " + errs);
    if (!root.isObject())
        throw Exception("Mustpass spec root must be a JSON object: " + specPath);

    Spec spec;
    spec.outputBaseDir = requireString(root, "output_base_dir", "root");

    requireMember(root, "configs", "root");
    const Json::Value &configs = root["configs"];
    if (!configs.isArray())
        throw Exception("Mustpass spec 'configs' must be an array");

    for (Json::ArrayIndex ci = 0; ci < configs.size(); ++ci)
    {
        const Json::Value &cv  = configs[ci];
        const std::string ctxC = "configs[" + std::to_string(ci) + "]";
        if (!cv.isObject())
            throw Exception("Mustpass spec " + ctxC + ": must be an object");

        spec.configs.emplace_back();
        Config &cur     = spec.configs.back();
        cur.name        = requireString(cv, "name", ctxC);
        cur.outputFile  = requireString(cv, "output_file", ctxC);
        cur.groupSubDir = cv.get("group_subdir", "").asString();

        if (cv.isMember("split_groups"))
        {
            const Json::Value &sg = cv["split_groups"];
            if (!sg.isArray())
                throw Exception("Mustpass spec " + ctxC + ".split_groups: must be an array");
            for (const auto &i : sg)
                cur.splitGroups.push_back(i.asString());
        }

        if (cv.isMember("filters"))
        {
            const Json::Value &filters = cv["filters"];
            if (!filters.isArray())
                throw Exception("Mustpass spec " + ctxC + ".filters: must be an array");
            for (Json::ArrayIndex fi = 0; fi < filters.size(); ++fi)
            {
                const Json::Value &fv  = filters[fi];
                const std::string ctxF = ctxC + ".filters[" + std::to_string(fi) + "]";
                if (!fv.isObject())
                    throw Exception("Mustpass spec " + ctxF + ": must be an object");

                cur.filters.emplace_back();
                Filter &cf              = cur.filters.back();
                const std::string tyStr = requireString(fv, "type", ctxF);
                if (tyStr == "include")
                    cf.type = Filter::TYPE_INCLUDE;
                else if (tyStr == "exclude")
                    cf.type = Filter::TYPE_EXCLUDE;
                else
                    throw Exception("Mustpass spec " + ctxF + ".type: must be 'include' or 'exclude'");

                if (fv.isMember("files"))
                {
                    const Json::Value &files = fv["files"];
                    if (!files.isArray())
                        throw Exception("Mustpass spec " + ctxF + ".files: must be an array");
                    for (const auto &file : files)
                        loadPatternsFromFile(file.asString(), cf.patterns);
                }
                if (fv.isMember("patterns"))
                {
                    const Json::Value &pats = fv["patterns"];
                    if (!pats.isArray())
                        throw Exception("Mustpass spec " + ctxF + ".patterns: must be an array");
                    for (const auto &pat : pats)
                        cf.patterns.add(pat.asString());
                }
            }
        }
    }
    return spec;
}

// Per-test routing.  Mirrors scripts/mustpass.py:processConfig.

std::vector<std::string> splitDots(const std::string &s)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s)
    {
        if (c == '.')
        {
            out.push_back(cur);
            cur.clear();
        }
        else
            cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

std::string replaceUnderscores(std::string s)
{
    std::replace(s.begin(), s.end(), '_', '-');
    return s;
}

// Returns the per-group sub-file path and the matching index entry for
// `caseName` under a split config, or false if the case has fewer than 3
// dot-segments.
bool routeSplitGroup(const Config &cfg, const std::string &caseName, std::string &subFileRelPathOut,
                     std::string &indexEntryOut)
{
    const std::vector<std::string> parts = splitDots(caseName);
    if (parts.size() < 3)
        return false;

    // Pick the deepest splitPattern that matches caseName, so a deeper entry
    // like "dEQP-VK.pipeline.monolithic" subdivides into per-subgroup files
    // instead of collapsing back into one with the shallower match.
    size_t bestDepth = 0;
    for (const std::string &splitPattern : cfg.splitGroups)
    {
        const std::vector<std::string> splitParts = splitDots(splitPattern);
        const size_t depth                        = splitParts.size();
        if (depth <= 1 || depth <= bestDepth || parts.size() <= depth)
            continue;
        const std::string prefix = splitPattern + ".";
        if (caseName.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), caseName.begin()))
            bestDepth = depth;
    }
    std::string groupName = replaceUnderscores(parts[1]);
    for (size_t i = 2; i <= bestDepth; ++i)
    {
        groupName += "/";
        groupName += replaceUnderscores(parts[i]);
    }
    const std::string fileName = groupName + ".txt";
    subFileRelPathOut          = cfg.groupSubDir + "/" + fileName;
    indexEntryOut              = cfg.groupSubDir + "/" + fileName;
    return true;
}

// Walk filters in source order: includes set keep=true on a hit, excludes
// set keep=false on a hit; short-circuit when keep is false.
bool evaluateConfig(Config &cfg, const std::string &caseName)
{
    bool keep = true;
    for (Filter &filter : cfg.filters)
    {
        if (filter.type == Filter::TYPE_INCLUDE)
        {
            keep = filter.patterns.match(caseName);
        }
        else
        {
            if (filter.patterns.match(caseName))
                keep = false;
        }
        if (!keep)
            break;
    }
    return keep;
}

// Tree walk + output emission

std::string joinPath(const std::string &a, const std::string &b)
{
    if (a.empty())
        return b;
    if (b.empty())
        return a;
    if (a.back() == '/')
        return a + b;
    return a + "/" + b;
}

// Open `sink` for `absPath`, creating parent dirs.
void openSink(OutputSink &sink, const std::string &absPath)
{
    const de::FilePath dir(de::FilePath(absPath).getDirName());
    if (!dir.exists())
        de::createDirectoryAndParents(dir.getPath());
    sink.out.open(absPath.c_str(), std::ios_base::binary);
    if (!sink.out.is_open() || !sink.out.good())
        throw Exception("Failed to open " + absPath);
}

// Append a line to cfg's sink for `absPath`, opening it lazily on first
// use.  Tree walk emits cases in alphabetical-by-name order at every level
// (TestNode::addChild is a sorted insert), so streaming straight to disk
// yields the desired order with no post-pass sort.
void appendLine(Config &cfg, const std::string &absPath, const std::string &line)
{
    auto [it, inserted] = cfg.outputs.try_emplace(absPath);
    if (inserted)
    {
        cfg.outputOrder.push_back(absPath);
        openSink(it->second, absPath);
    }
    it->second.out << line << "\n";
}

// Write a small one-shot file (used for the split-group index).
void writeIndexFile(const std::string &absPath, const std::vector<std::string> &lines)
{
    const de::FilePath dir(de::FilePath(absPath).getDirName());
    if (!dir.exists())
        de::createDirectoryAndParents(dir.getPath());
    std::ofstream out(absPath.c_str(), std::ios_base::binary);
    if (!out.is_open() || !out.good())
        throw Exception("Failed to open " + absPath);
    for (const std::string &l : lines)
        out << l << "\n";
}

void runWalk(TestPackageRoot &root, TestContext &testCtx, Spec &spec)
{
    DefaultHierarchyInflater inflater(testCtx);
    de::MovePtr<const CaseListFilter> caseListFilter(
        testCtx.getCommandLine().createCaseListFilter(testCtx.getArchive()));

    TestHierarchyIterator iter(root, inflater, *caseListFilter);

    // Iterate every package.
    while (iter.getState() != TestHierarchyIterator::STATE_FINISHED)
    {
        DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE &&
                  iter.getNode()->getNodeType() == NODETYPE_PACKAGE);
        try
        {
            iter.next();
        }
        catch (const tcu::NotSupportedError &)
        {
            return;
        }

        while (iter.getNode()->getNodeType() != NODETYPE_PACKAGE)
        {
            if (iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE &&
                isTestNodeTypeExecutable(iter.getNode()->getNodeType()))
            {
                const std::string &caseName = iter.getNodePath();
                for (Config &cfg : spec.configs)
                {
                    if (!evaluateConfig(cfg, caseName))
                        continue;

                    if (cfg.splitGroups.empty())
                    {
                        // Stream directly to the main output file.
                        const std::string absPath = joinPath(spec.outputBaseDir, cfg.outputFile);
                        appendLine(cfg, absPath, caseName);
                    }
                    else
                    {
                        std::string subRel, indexEntry;
                        if (routeSplitGroup(cfg, caseName, subRel, indexEntry))
                        {
                            const std::string absPath = joinPath(spec.outputBaseDir, subRel);
                            appendLine(cfg, absPath, caseName);
                        }
                        // Else: matches Python behaviour of silently dropping
                        // case names with fewer than 3 dot-segments under a
                        // split config.
                    }
                }
            }
            iter.next();
        }

        DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_LEAVE_NODE &&
                  iter.getNode()->getNodeType() == NODETYPE_PACKAGE);
        iter.next();
    }
}

void writeOutputs(Spec &spec)
{
    const std::string baseAbs = spec.outputBaseDir;
    for (Config &cfg : spec.configs)
    {
        if (cfg.splitGroups.empty())
        {
            // The main file was streamed during the walk.  Open it now if
            // no case routed there, so we still produce an empty file.
            const std::string mainAbs = joinPath(baseAbs, cfg.outputFile);
            auto [it, inserted]       = cfg.outputs.try_emplace(mainAbs);
            if (inserted)
                openSink(it->second, mainAbs);
            it->second.out.close();
            qpPrintf("%s\n", mainAbs.c_str());
        }
        else
        {
            // Per-group sub-files were streamed during the walk; close them
            // in first-touch order and emit the matching index file.
            const std::string mainAbs = joinPath(baseAbs, cfg.outputFile);
            std::vector<std::string> indexEntries;
            indexEntries.reserve(cfg.outputOrder.size());
            for (const std::string &subAbs : cfg.outputOrder)
            {
                qpPrintf("    %s\n", subAbs.c_str());
                cfg.outputs[subAbs].out.close();
                DE_ASSERT(subAbs.size() > baseAbs.size() + 1);
                indexEntries.push_back(subAbs.substr(baseAbs.size() + 1));
            }
            qpPrintf("%s\n", mainAbs.c_str());
            writeIndexFile(mainAbs, indexEntries);
        }
    }
}

} // namespace

/*--------------------------------------------------------------------*//*!
 * \brief Generate mustpass case-list files from a JSON spec.
 *
 * Reads the spec at --deqp-mustpass-spec=<path>, walks the test hierarchy
 * once, and streams the kept case names to one or more output files per
 * configuration declared in the spec.
 *//*--------------------------------------------------------------------*/
void genMustpassFromSpec(TestPackageRoot &root, TestContext &testCtx, const CommandLine &cmdLine)
{
    const std::string specPath = cmdLine.getMustpassSpec();
    if (specPath.empty())
        throw Exception("--deqp-runmode=gen-mustpass requires --deqp-mustpass-spec=<path>");

    Spec spec = parseSpec(specPath);
    if (spec.outputBaseDir.empty())
        throw Exception("Mustpass spec missing 'output_base_dir'");
    if (spec.configs.empty())
        throw Exception("Mustpass spec 'configs' is empty");

    runWalk(root, testCtx, spec);
    writeOutputs(spec);
}

} // namespace tcu
