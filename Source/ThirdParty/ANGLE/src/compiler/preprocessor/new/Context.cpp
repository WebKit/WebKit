//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Context.h"

#include <algorithm>
#include <sstream>

#include "compiler/debug.h"
#include "stl_utils.h"
#include "token_type.h"

static bool isMacroNameReserved(const std::string* name)
{
    ASSERT(name);

    // Names prefixed with "GL_" are reserved.
    if (name->substr(0, 3) == "GL_")
        return true;

    // Names containing two consecutive underscores are reserved.
    if (name->find("__") != std::string::npos)
        return true;

    return false;
}

namespace pp
{

Context::Context()
    : mLexer(NULL),
      mInput(NULL),
      mOutput(NULL)
{
}

Context::~Context()
{
    destroyLexer();
}

bool Context::init()
{
    return initLexer();

    // TODO(alokp): Define built-in macros here so that we do not need to
    // define them everytime in process().
}

bool Context::process(int count,
                      const char* const string[],
                      const int length[],
                      TokenVector* output)
{
    ASSERT((count >=0) && (string != NULL) && (output != NULL));

    // Setup.
    mInput = new Input(count, string, length);
    mOutput = output;
    defineBuiltInMacro("GL_ES", 1);

    // Parse.
    bool success = parse();

    // Cleanup.
    reset();
    return success;
}

bool Context::defineMacro(pp::Token::Location location,
                          pp::Macro::Type type,
                          std::string* name,
                          pp::TokenVector* parameters,
                          pp::TokenVector* replacements)
{
    std::auto_ptr<Macro> macro(new Macro(type, name, parameters, replacements));
    if (isMacroNameReserved(name))
    {
        // TODO(alokp): Report error.
        return false;
    }
    if (isMacroDefined(name))
    {
        // TODO(alokp): Report error.
        return false;
    }

    mMacros[*name] = macro.release();
    return true;
}

bool Context::undefineMacro(const std::string* name)
{
    MacroSet::iterator iter = mMacros.find(*name);
    if (iter == mMacros.end())
    {
        // TODO(alokp): Report error.
        return false;
    }
    mMacros.erase(iter);
    return true;
}

bool Context::isMacroDefined(const std::string* name)
{
    return mMacros.find(*name) != mMacros.end();
}

// Reset to initialized state.
void Context::reset()
{
    std::for_each(mMacros.begin(), mMacros.end(), DeleteSecond());
    mMacros.clear();

    delete mInput;
    mInput = NULL;

    mOutput = NULL;
}

void Context::defineBuiltInMacro(const std::string& name, int value)
{
    std::ostringstream stream;
    stream << value;
    Token* token = new Token(0, INT_CONSTANT, new std::string(stream.str()));
    TokenVector* replacements = new pp::TokenVector(1, token);

    mMacros[name] = new Macro(Macro::kTypeObj,
                              new std::string(name),
                              NULL,
                              replacements);
}

}  // namespace pp

