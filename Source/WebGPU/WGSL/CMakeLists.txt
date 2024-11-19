# Copyright 2024 Google LLC
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Build wgslc.
# For simplicity, build it as a monolithic executable.

# Target 'wgsl-types' generates TypeDeclarations.h in the build tree.
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/TypeDeclarations.h
    COMMAND ${RUBY_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/generator/main.rb
        ${CMAKE_CURRENT_SOURCE_DIR}/TypeDeclarations.rb
        ${CMAKE_CURRENT_BINARY_DIR}/TypeDeclarations.h
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/generator/main.rb
        ${CMAKE_CURRENT_SOURCE_DIR}/TypeDeclarations.rb
    )
add_custom_target(wgsl-types
    ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/TypeDeclarations.h)

set(wgslc_SOURCES
    AttributeValidator.cpp
    BoundsCheck.cpp
    CallGraph.cpp
    CompilationMessage.cpp
    CompilationScope.cpp
    ConstantValue.cpp
    Constraints.cpp
    EntryPointRewriter.cpp
    GlobalSorting.cpp
    GlobalVariableRewriter.cpp
    Lexer.cpp
    MangleNames.cpp
    Overload.cpp
    Parser.cpp
    PointerRewriter.cpp
    Token.cpp
    TypeCheck.cpp
    TypeStore.cpp
    Types.cpp
    VisibilityValidator.cpp
    WGSL.cpp
    WGSLEnums.cpp
    WGSLShaderModule.cpp
    wgslc.cpp

    AST/ASTBinaryExpression.cpp
    AST/ASTBuilder.cpp
    AST/ASTDecrementIncrementStatement.cpp
    AST/ASTStringDumper.cpp
    AST/ASTUnaryExpression.cpp
    AST/ASTVisitor.cpp

    Metal/MetalCodeGenerator.cpp
    Metal/MetalFunctionWriter.cpp
    )

set(wgslc_HEADERS
    API.h
    AttributeValidator.h
    BoundsCheck.h
    CallGraph.h
    CompilationMessage.h
    CompilationScope.h
    ConstantFunctions.h
    ConstantValue.h
    Constraints.h
    ContextProvider.h
    ContextProviderInlines.h
    EntryPointRewriter.h
    GlobalSorting.h
    GlobalVariableRewriter.h
    Lexer.h
    MangleNames.h
    Overload.h
    Parser.h
    ParserPrivate.h
    PhaseTimer.h
    PointerRewriter.h
    SourceSpan.h
    Token.h
    TypeCheck.h
    TypeStore.h
    Types.h
    VisibilityValidator.h
    WGSL.h
    WGSLEnums.h
    WGSLShaderModule.h
    config.h

    AST/AST.h
    AST/ASTAbstractFloatLiteral.h
    AST/ASTAbstractIntegerLiteral.h
    AST/ASTAlignAttribute.h
    AST/ASTArrayTypeExpression.h
    AST/ASTAssignmentStatement.h
    AST/ASTAttribute.h
    AST/ASTBinaryExpression.h
    AST/ASTBindingAttribute.h
    AST/ASTBitcastExpression.h
    AST/ASTBoolLiteral.h
    AST/ASTBreakStatement.h
    AST/ASTBuilder.h
    AST/ASTBuiltinAttribute.h
    AST/ASTCallExpression.h
    AST/ASTCallStatement.h
    AST/ASTCompoundAssignmentStatement.h
    AST/ASTCompoundStatement.h
    AST/ASTConstAssert.h
    AST/ASTConstAssertStatement.h
    AST/ASTConstAttribute.h
    AST/ASTContinueStatement.h
    AST/ASTDeclaration.h
    AST/ASTDecrementIncrementStatement.h
    AST/ASTDiagnostic.h
    AST/ASTDiagnosticAttribute.h
    AST/ASTDiagnosticDirective.h
    AST/ASTDirective.h
    AST/ASTDiscardStatement.h
    AST/ASTElaboratedTypeExpression.h
    AST/ASTExpression.h
    AST/ASTFieldAccessExpression.h
    AST/ASTFloat16Literal.h
    AST/ASTFloat32Literal.h
    AST/ASTForStatement.h
    AST/ASTForward.h
    AST/ASTFunction.h
    AST/ASTGroupAttribute.h
    AST/ASTIdAttribute.h
    AST/ASTIdentifier.h
    AST/ASTIdentifierExpression.h
    AST/ASTIdentityExpression.h
    AST/ASTIfStatement.h
    AST/ASTIndexAccessExpression.h
    AST/ASTInterpolateAttribute.h
    AST/ASTInvariantAttribute.h
    AST/ASTLocationAttribute.h
    AST/ASTLoopStatement.h
    AST/ASTMustUseAttribute.h
    AST/ASTNode.h
    AST/ASTParameter.h
    AST/ASTPhonyStatement.h
    AST/ASTPointerDereference.h
    AST/ASTReferenceTypeExpression.h
    AST/ASTReturnStatement.h
    AST/ASTScopedVisitor.h
    AST/ASTScopedVisitorInlines.h
    AST/ASTSigned32Literal.h
    AST/ASTSizeAttribute.h
    AST/ASTStageAttribute.h
    AST/ASTStatement.h
    AST/ASTStaticAssertStatement.h
    AST/ASTStringDumper.h
    AST/ASTStructure.h
    AST/ASTStructureMember.h
    AST/ASTSwitchStatement.h
    AST/ASTTypeAlias.h
    AST/ASTUnaryExpression.h
    AST/ASTUnsigned32Literal.h
    AST/ASTVariable.h
    AST/ASTVariableQualifier.h
    AST/ASTVariableStatement.h
    AST/ASTVisitor.h
    AST/ASTWhileStatement.h
    AST/ASTWorkgroupSizeAttribute.h

    Metal/MetalCodeGenerator.h
    Metal/MetalFunctionWriter.h
    )

set(wgslc_FRAMEWORKS WTF bmalloc)
set(wgslc_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_BINARY_DIR}/../../..  # for cmakeconfig.h
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/AST
    ${CMAKE_CURRENT_SOURCE_DIR}/Metal
    ${GLIB_INCLUDE_DIR}
    ${GLIBCONFIG_INCLUDE_DIR})
set(wgslc_DEPENDENCIES wgsl-types)

WEBKIT_EXECUTABLE_DECLARE(wgslc)
WEBKIT_EXECUTABLE(wgslc)

install(TARGETS wgslc EXCLUDE_FROM_ALL)
