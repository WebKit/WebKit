//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_CONTEXT_H_
#define COMPILER_PREPROCESSOR_CONTEXT_H_

#include <map>

#include "common/angleutils.h"
#include "Input.h"
#include "Macro.h"
#include "Token.h"

namespace pp
{

class Context
{
  public:
    Context();
    ~Context();

    bool init();
    bool process(int count, const char* const string[], const int length[],
                 TokenVector* output);

    void* lexer() { return mLexer; }
    int readInput(char* buf, int maxSize);
    TokenVector* output() { return mOutput; }

    bool defineMacro(pp::Token::Location location,
                     pp::Macro::Type type,
                     std::string* name,
                     pp::TokenVector* parameters,
                     pp::TokenVector* replacements);
    bool undefineMacro(const std::string* name);
    bool isMacroDefined(const std::string* name);

  private:
    DISALLOW_COPY_AND_ASSIGN(Context);
    typedef std::map<std::string, Macro*> MacroSet;

    void reset();
    bool initLexer();
    void destroyLexer();
    void defineBuiltInMacro(const std::string& name, int value);
    bool parse();

    void* mLexer;  // Lexer handle.
    Input* mInput;
    TokenVector* mOutput;
    MacroSet mMacros;  // Defined macros.
};

}  // namespace pp
#endif  // COMPILER_PREPROCESSOR_CONTEXT_H_

