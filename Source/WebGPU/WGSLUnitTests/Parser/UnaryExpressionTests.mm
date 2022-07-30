#import <XCTest/XCTest.h>

#include "Parser.h"
#include "UnaryExpression.h"

static Expected<UniqueRef<WGSL::AST::Expression>, WGSL::Error> parseLCharUnaryExpression(const String& input)
{
    WGSL::Lexer<LChar> lexer(input);
    WGSL::Parser parser(lexer);

    return parser.parseUnaryExpression();
}

static void testOneUnaryOperation(const String& type, WGSL::AST::UnaryExpression::Op expectedOperation) {
    auto exprResult = parseLCharUnaryExpression(type);
    XCTAssert(exprResult);
    const auto& expr = (*exprResult).get();

    XCTAssert(is<WGSL::AST::UnaryExpression>(expr));
    const auto& unaryExpr = downcast<WGSL::AST::UnaryExpression>(expr);

    XCTAssert(unaryExpr.op() == expectedOperation);
}

@interface UnaryExpressionTests : XCTestCase

@end

@implementation UnaryExpressionTests

- (void)setUp {
    [super setUp];
    [self setContinueAfterFailure:NO];
}

- (void)testLogicalNot {
    testOneUnaryOperation("!1"_s, WGSL::AST::UnaryExpression::Op::LogicalNot);
}

- (void)testBitwiseNot {
    testOneUnaryOperation("~1"_s, WGSL::AST::UnaryExpression::Op::BitwiseNot);
}

- (void)testNegation {
    testOneUnaryOperation("-1"_s, WGSL::AST::UnaryExpression::Op::Negation);
}

- (void)testAddressOf {
    testOneUnaryOperation("&identifier"_s, WGSL::AST::UnaryExpression::Op::AddressOf);
}

- (void)testIndirection {
    testOneUnaryOperation("*identifier"_s, WGSL::AST::UnaryExpression::Op::Indirection);
}

- (void)testPrecedence {
    auto exprResult = parseLCharUnaryExpression("!~1"_s);
    XCTAssert(exprResult);
    const auto& expr = (*exprResult).get();

    XCTAssert(is<WGSL::AST::UnaryExpression>(expr));
    const auto& unaryExpr = downcast<WGSL::AST::UnaryExpression>(expr);

    XCTAssert(unaryExpr.op() == WGSL::AST::UnaryExpression::Op::LogicalNot);

    const auto& innerExpr = unaryExpr.expression();
    XCTAssert(is<WGSL::AST::UnaryExpression>(expr));
    const auto& innerUnaryExpr = downcast<WGSL::AST::UnaryExpression>(innerExpr);
    XCTAssert(innerUnaryExpr.op() == WGSL::AST::UnaryExpression::Op::BitwiseNot);
}

@end
