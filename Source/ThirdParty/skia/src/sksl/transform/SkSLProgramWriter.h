/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSLProgramWriter_DEFINED
#define SkSLProgramWriter_DEFINED

#include "src/sksl/analysis/SkSLProgramVisitor.h"

namespace SkSL {

struct ProgramWriterTypes {
    using Program = SkSL::Program;
    using Expression = SkSL::Expression;
    using Statement = SkSL::Statement;
    using ProgramElement = SkSL::ProgramElement;
    using UniquePtrExpression = std::unique_ptr<SkSL::Expression>;
    using UniquePtrStatement = std::unique_ptr<SkSL::Statement>;
};

extern template class TProgramVisitor<ProgramWriterTypes>;

class ProgramWriter : public TProgramVisitor<ProgramWriterTypes> {
public:
    // Subclass these methods if you want access to the unique_ptrs of IRNodes in a program.
    // This will allow statements or expressions to be replaced during a visit.
    bool visitExpressionPtr(std::unique_ptr<Expression>& e) override {
        return this->visitExpression(*e);
    }
    bool visitStatementPtr(std::unique_ptr<Statement>& s) override {
        return this->visitStatement(*s);
    }
};

} // namespace SkSL

#endif
