//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_MACROEXPANDER_H_
#define COMPILER_PREPROCESSOR_MACROEXPANDER_H_

#include <memory>
#include <vector>

#include "compiler/preprocessor/Lexer.h"
#include "compiler/preprocessor/Macro.h"

namespace pp
{

class Diagnostics;
struct SourceLocation;

class MacroExpander : public Lexer
{
  public:
    MacroExpander(Lexer *lexer, MacroSet *macroSet, Diagnostics *diagnostics);
    ~MacroExpander() override;

    void lex(Token *token) override;

  private:
    void getToken(Token *token);
    void ungetToken(const Token &token);
    bool isNextTokenLeftParen();

    bool pushMacro(const Macro &macro, const Token &identifier);
    void popMacro();

    bool expandMacro(const Macro &macro,
                     const Token &identifier,
                     std::vector<Token> *replacements);

    typedef std::vector<Token> MacroArg;
    bool collectMacroArgs(const Macro &macro,
                          const Token &identifier,
                          std::vector<MacroArg> *args,
                          SourceLocation *closingParenthesisLocation);
    void replaceMacroParams(const Macro &macro,
                            const std::vector<MacroArg> &args,
                            std::vector<Token> *replacements);

    struct MacroContext
    {
        MacroContext();
        bool empty() const;
        const Token &get();
        void unget();

        const Macro *macro;
        std::size_t index;
        std::vector<Token> replacements;
    };

    Lexer *mLexer;
    MacroSet *mMacroSet;
    Diagnostics *mDiagnostics;

    std::unique_ptr<Token> mReserveToken;
    std::vector<MacroContext *> mContextStack;
    size_t mTotalTokensInContexts;
};

}  // namespace pp

#endif  // COMPILER_PREPROCESSOR_MACROEXPANDER_H_
