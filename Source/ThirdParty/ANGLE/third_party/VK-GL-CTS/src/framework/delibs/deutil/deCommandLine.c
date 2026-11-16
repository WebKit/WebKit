/*-------------------------------------------------------------------------
 * drawElements Utility Library
 * ----------------------------
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
 * \brief Command line parser.
 *//*--------------------------------------------------------------------*/

#include "deCommandLine.h"
#include "deMemPool.h"
#include "dePoolArray.h"
#include "deMemory.h"
#include "deString.h"

#include <string.h>

DE_DECLARE_POOL_ARRAY(CharPtrArray, char *);

enum
{
    MAX_ARGS = 64
};

deCommandLine *deCommandLine_parse(const char *commandLine)
{
    deMemPool *tmpPool = deMemPool_createRoot(NULL, 0);
    CharPtrArray *args = tmpPool ? CharPtrArray_create(tmpPool) : NULL;
    char *buf          = NULL;
    char *outPtr;
    int pos;
    int argNdx;
    char strChr;

    if (!args)
    {
        if (tmpPool)
            deMemPool_destroy(tmpPool);
        return NULL;
    }

    DE_ASSERT(commandLine);

    /* Create buffer for args (no expansion can happen). */
    buf    = (char *)deCalloc(strlen(commandLine) + 1);
    pos    = 0;
    argNdx = 0;
    outPtr = buf;
    strChr = 0;

    if (!buf || !CharPtrArray_pushBack(args, buf))
    {
        deMemPool_destroy(tmpPool);
        return NULL;
    }

    while (commandLine[pos] != 0)
    {
        char c = commandLine[pos++];

        if (strChr != 0 && c == '\\')
        {
            /* Escape. */
            c = commandLine[pos++];
            switch (c)
            {
            case 'n':
                *outPtr++ = '\n';
                break;
            case 't':
                *outPtr++ = '\t';
                break;
            default:
                *outPtr++ = c;
                break;
            }
        }
        else if (strChr != 0 && c == strChr)
        {
            /* String end. */
            strChr = 0;
        }
        else if (strChr == 0 && (c == '"' || c == '\''))
        {
            /* String start. */
            strChr = c;
        }
        else if (c == ' ' && strChr == 0)
        {
            /* Arg end. */
            *outPtr++ = 0;
            argNdx += 1;
            if (!CharPtrArray_pushBack(args, outPtr))
            {
                deFree(buf);
                deMemPool_destroy(tmpPool);
                return NULL;
            }
        }
        else
            *outPtr++ = c;
    }

    DE_ASSERT(commandLine[pos] == 0);

    /* Terminate last arg. */
    *outPtr = 0;

    /* Create deCommandLine. */
    {
        deCommandLine *cmdLine = (deCommandLine *)deCalloc(sizeof(deCommandLine));

        if (!cmdLine ||
            !(cmdLine->args = (char **)deCalloc(sizeof(char *) * (size_t)CharPtrArray_getNumElements(args))))
        {
            deFree(cmdLine);
            deFree(buf);
            deMemPool_destroy(tmpPool);
            return NULL;
        }

        cmdLine->numArgs = CharPtrArray_getNumElements(args);
        cmdLine->argBuf  = buf;

        for (argNdx = 0; argNdx < cmdLine->numArgs; argNdx++)
            cmdLine->args[argNdx] = CharPtrArray_get(args, argNdx);

        deMemPool_destroy(tmpPool);
        return cmdLine;
    }
}

void deCommandLine_destroy(deCommandLine *cmdLine)
{
    deFree(cmdLine->argBuf);
    deFree(cmdLine);
}

static void testParse(const char *cmdLine, const char *const *refArgs, int numArgs)
{
    deCommandLine *parsedCmdLine = deCommandLine_parse(cmdLine);
    int argNdx;

    DE_TEST_ASSERT(parsedCmdLine);
    DE_TEST_ASSERT(parsedCmdLine->numArgs == numArgs);

    for (argNdx = 0; argNdx < numArgs; argNdx++)
        DE_TEST_ASSERT(strcmp(parsedCmdLine->args[argNdx], refArgs[argNdx]) == 0);

    deCommandLine_destroy(parsedCmdLine);
}

void deCommandLine_selfTest(void)
{
    {
        const char *cmdLine = "hello";
        const char *ref[]   = {"hello"};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
    {
        const char *cmdLine = "hello world";
        const char *ref[]   = {"hello", "world"};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
    {
        const char *cmdLine = "hello/world";
        const char *ref[]   = {"hello/world"};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
    {
        const char *cmdLine = "hello/world --help";
        const char *ref[]   = {"hello/world", "--help"};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
    {
        const char *cmdLine = "hello/world --help foo";
        const char *ref[]   = {"hello/world", "--help", "foo"};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
    {
        const char *cmdLine = "hello\\world --help foo";
        const char *ref[]   = {"hello\\world", "--help", "foo"};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
    {
        const char *cmdLine = "\"hello/worl d\" --help --foo=\"bar\" \"ba z\\\"\"";
        const char *ref[]   = {"hello/worl d", "--help", "--foo=bar", "ba z\""};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
    {
        const char *cmdLine = "'hello/worl d' --help --foo='bar' 'ba z\\\''";
        const char *ref[]   = {"hello/worl d", "--help", "--foo=bar", "ba z'"};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
    {
        const char *cmdLine = "hello \"'world'\"";
        const char *ref[]   = {"hello", "'world'"};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
    {
        const char *cmdLine = "hello '\"world\"'";
        const char *ref[]   = {"hello", "\"world\""};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
    {
        const char *cmdLine = "hello \"world\\n\"";
        const char *ref[]   = {"hello", "world\n"};
        testParse(cmdLine, ref, DE_LENGTH_OF_ARRAY(ref));
    }
}
