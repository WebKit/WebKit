/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Test hierarchy utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuTestHierarchyUtil.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuCommandLine.hpp"

#include "qpXmlWriter.h"

#include <fstream>

namespace tcu
{

using std::string;

static const char *getNodeTypeName(TestNodeType nodeType)
{
    switch (nodeType)
    {
    case NODETYPE_SELF_VALIDATE:
        return "SelfValidate";
    case NODETYPE_CAPABILITY:
        return "Capability";
    case NODETYPE_ACCURACY:
        return "Accuracy";
    case NODETYPE_PERFORMANCE:
        return "Performance";
    case NODETYPE_GROUP:
        return "TestGroup";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

// Utilities

static std::string makePackageFilename(const std::string &pattern, const std::string &packageName,
                                       const std::string &typeExtension)
{
    std::map<string, string> args;
    args["packageName"]   = packageName;
    args["typeExtension"] = typeExtension;
    return StringTemplate(pattern).specialize(args);
}

static void writeXmlCaselist(TestHierarchyIterator &iter, qpXmlWriter *writer)
{
    DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE &&
              iter.getNode()->getNodeType() == NODETYPE_PACKAGE);

    {
        const TestNode *node = iter.getNode();
        qpXmlAttribute attribs[1];
        int numAttribs        = 0;
        attribs[numAttribs++] = qpSetStringAttrib("PackageName", node->getName());
        DE_ASSERT(numAttribs <= DE_LENGTH_OF_ARRAY(attribs));

        if (!qpXmlWriter_startDocument(writer, true) ||
            !qpXmlWriter_startElement(writer, "TestCaseList", numAttribs, attribs))
            throw Exception("Failed to start XML document");
    }

    iter.next();

    while (iter.getNode()->getNodeType() != NODETYPE_PACKAGE)
    {
        const TestNode *const node  = iter.getNode();
        const TestNodeType nodeType = node->getNodeType();
        const bool isEnter          = iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE;

        DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE ||
                  iter.getState() == TestHierarchyIterator::STATE_LEAVE_NODE);
        {
            if (isEnter)
            {
                const string caseName = node->getName();
                qpXmlAttribute attribs[2];
                int numAttribs = 0;

                attribs[numAttribs++] = qpSetStringAttrib("Name", caseName.c_str());
                attribs[numAttribs++] = qpSetStringAttrib("CaseType", getNodeTypeName(nodeType));
                DE_ASSERT(numAttribs <= DE_LENGTH_OF_ARRAY(attribs));

                if (!qpXmlWriter_startElement(writer, "TestCase", numAttribs, attribs))
                    throw Exception("Writing to case list file failed");
            }
            else
            {
                if (!qpXmlWriter_endElement(writer, "TestCase"))
                    throw tcu::Exception("Writing to case list file failed");
            }
        }

        iter.next();
    }

    // This could be done in catch, but the file is corrupt at that point anyways.
    if (!qpXmlWriter_endElement(writer, "TestCaseList") || !qpXmlWriter_endDocument(writer))
        throw Exception("Failed to terminate XML document");
}

/*--------------------------------------------------------------------*//*!
 * \brief Export the test list of each package into a separate XML file.
 *//*--------------------------------------------------------------------*/
void writeXmlCaselistsToFiles(TestPackageRoot &root, TestContext &testCtx, const CommandLine &cmdLine)
{
    DefaultHierarchyInflater inflater(testCtx);
    de::MovePtr<const CaseListFilter> caseListFilter(
        testCtx.getCommandLine().createCaseListFilter(testCtx.getArchive()));

    TestHierarchyIterator iter(root, inflater, *caseListFilter);
    const char *const filenamePattern = cmdLine.getCaseListExportFile();

    while (iter.getState() != TestHierarchyIterator::STATE_FINISHED)
    {
        const TestNode *node  = iter.getNode();
        const char *pkgName   = node->getName();
        const string filename = makePackageFilename(filenamePattern, pkgName, "xml");

        DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE &&
                  node->getNodeType() == NODETYPE_PACKAGE);

        FILE *file          = nullptr;
        qpXmlWriter *writer = nullptr;

        try
        {
            file = fopen(filename.c_str(), "wb");
            if (!file)
                throw Exception("Failed to open " + filename);

            writer = qpXmlWriter_createFileWriter(file, false, false);
            if (!writer)
                throw Exception("XML writer creation failed");

            print("Writing test cases from '%s' to file '%s'..\n", pkgName, filename.c_str());

            writeXmlCaselist(iter, writer);

            qpXmlWriter_destroy(writer);
            writer = nullptr;

            fclose(file);
            file = nullptr;
        }
        catch (...)
        {
            if (writer)
                qpXmlWriter_destroy(writer);
            if (file)
                fclose(file);
            throw;
        }

        DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_LEAVE_NODE &&
                  iter.getNode()->getNodeType() == NODETYPE_PACKAGE);
        iter.next();
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Export the test list of each package into a separate ascii file.
 *//*--------------------------------------------------------------------*/
void writeTxtCaselistsToFiles(TestPackageRoot &root, TestContext &testCtx, const CommandLine &cmdLine, bool printPrefix,
                              bool skipGroups)
{
    DefaultHierarchyInflater inflater(testCtx);
    de::MovePtr<const CaseListFilter> caseListFilter(
        testCtx.getCommandLine().createCaseListFilter(testCtx.getArchive()));

    TestHierarchyIterator iter(root, inflater, *caseListFilter);
    const char *const filenamePattern = cmdLine.getCaseListExportFile();

    while (iter.getState() != TestHierarchyIterator::STATE_FINISHED)
    {
        const TestNode *node  = iter.getNode();
        const char *pkgName   = node->getName();
        const string filename = makePackageFilename(filenamePattern, pkgName, "txt");

        DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE &&
                  node->getNodeType() == NODETYPE_PACKAGE);

        std::ofstream out(filename.c_str(), std::ios_base::binary);
        if (!out.is_open() || !out.good())
            throw Exception("Failed to open " + filename);

        print("Writing test cases from '%s' to file '%s'..\n", pkgName, filename.c_str());

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
            if (iter.getState() == TestHierarchyIterator::STATE_ENTER_NODE)
            {
                const bool isTest = isTestNodeTypeExecutable(iter.getNode()->getNodeType());
                if (isTest || !skipGroups)
                {
                    const char *prefix = (printPrefix ? (isTest ? "TEST: " : "GROUP: ") : "");
                    out << prefix << iter.getNodePath() << "\n";
                }
            }
            iter.next();
        }

        DE_ASSERT(iter.getState() == TestHierarchyIterator::STATE_LEAVE_NODE &&
                  iter.getNode()->getNodeType() == NODETYPE_PACKAGE);
        iter.next();
    }
}

} // namespace tcu
