#import <XCTest/XCTest.h>

#include "BinaryExpression.h"
#include "IdentifierExpression.h"
#include "LiteralExpressions.h"
#include "Parser.h"
#include "StructureAccess.h"

static Expected<UniqueRef<WGSL::AST::Expression>, WGSL::Error> parseLCharSingularExpression(const String& input)
{
    WGSL::Lexer<LChar> lexer(input);
    WGSL::Parser parser(lexer);

    return parser.parseSingularExpression();
}

@interface SingularExpressionTests : XCTestCase

@end

@implementation SingularExpressionTests

- (void)setUp {
    [super setUp];
    [self setContinueAfterFailure:NO];
}

- (void)testStructureAccess {
    auto parseResult = parseLCharSingularExpression("web.gpu"_s);
    XCTAssert(parseResult);
    auto expr = WTFMove(*parseResult);

    XCTAssert(is<WGSL::AST::StructureAccess>(expr));
    const auto& structAccessExpr = downcast<WGSL::AST::StructureAccess>(expr.get());

    // Check left side.
    const auto& lhsExpr = structAccessExpr.base();
    XCTAssert(is<WGSL::AST::IdentifierExpression>(lhsExpr));
    const auto& lhsLiteral = downcast<WGSL::AST::IdentifierExpression>(lhsExpr);
    XCTAssert(lhsLiteral.identifier() == "web"_s);

    // Check right side.
    const auto& rhsIdent = structAccessExpr.fieldName();
    XCTAssert(rhsIdent == "gpu"_s);
}

- (void)test1DSubscriptAccess {
    auto parseResult = parseLCharSingularExpression("webkit[0]"_s);
    XCTAssert(parseResult);
    auto expr = WTFMove(*parseResult);

    XCTAssert(is<WGSL::AST::BinaryExpression>(expr));
    const auto& subscriptExpr = downcast<WGSL::AST::BinaryExpression>(expr.get());
    
    // Check correct operator
    XCTAssert(subscriptExpr.op() == WGSL::AST::BinaryExpression::Op::Subscript);

    // Check left side.
    const auto& lhsExpr = subscriptExpr.lhs();
    XCTAssert(is<WGSL::AST::IdentifierExpression>(lhsExpr));
    const auto& lhsIdent = downcast<WGSL::AST::IdentifierExpression>(lhsExpr);
    XCTAssert(lhsIdent.identifier() == "webkit"_s);

    // Check right side.
    const auto& rhsExpr = subscriptExpr.rhs();
    XCTAssert(is<WGSL::AST::AbstractIntLiteral>(rhsExpr));
    const auto& rhsLiteral = downcast<WGSL::AST::AbstractIntLiteral>(rhsExpr);
    XCTAssert(rhsLiteral.value() == 0);
}

- (void)test2DSubscriptAccess {
    auto parseResult = parseLCharSingularExpression("webkit[0][1]"_s);
    XCTAssert(parseResult);
    auto expr = WTFMove(*parseResult);

    XCTAssert(is<WGSL::AST::BinaryExpression>(expr));
    const auto& subscriptExpr = downcast<WGSL::AST::BinaryExpression>(expr.get());

    // Check left side, which is "webkit[0]"
    {
        const auto& lhsExpr = subscriptExpr.lhs();
        XCTAssert(is<WGSL::AST::BinaryExpression>(lhsExpr));

        const auto& lhsSubscriptExpr = downcast<WGSL::AST::BinaryExpression>(lhsExpr);
        XCTAssert(lhsSubscriptExpr.op() == WGSL::AST::BinaryExpression::Op::Subscript);

        // Check left side of "webkit[0]", which should be "webkit"
        {
            const auto& lhsExpr = lhsSubscriptExpr.lhs();
            XCTAssert(is<WGSL::AST::IdentifierExpression>(lhsExpr));
            const auto& lhsIdent = downcast<WGSL::AST::IdentifierExpression>(lhsExpr);
            XCTAssert(lhsIdent.identifier() == "webkit"_s);
        }

        // Check right side of "webkit[0]", which should be "0"
        {
            const auto& rhsExpr = lhsSubscriptExpr.rhs();
            XCTAssert(is<WGSL::AST::AbstractIntLiteral>(rhsExpr));
            const auto& rhsLiteral = downcast<WGSL::AST::AbstractIntLiteral>(rhsExpr);
            XCTAssert(rhsLiteral.value() == 0);
        }
    }

    // Check right side.
    const auto& rhsExpr = subscriptExpr.rhs();
    XCTAssert(is<WGSL::AST::AbstractIntLiteral>(rhsExpr));
    const auto& rhsLiteral = downcast<WGSL::AST::AbstractIntLiteral>(rhsExpr);
    XCTAssert(rhsLiteral.value() == 1);
}

- (void)testStructureAndSubscriptAccess {
    auto parseResult = parseLCharSingularExpression("webkit[0].webcore"_s);
    XCTAssert(parseResult);
    auto expr = WTFMove(*parseResult);

    XCTAssert(is<WGSL::AST::StructureAccess>(expr));
    const auto& structAccessExpr = downcast<WGSL::AST::StructureAccess>(expr.get());

    // Check left side, which is "webkit[0]"
    {
        const auto& lhsExpr = structAccessExpr.base();
        XCTAssert(is<WGSL::AST::BinaryExpression>(lhsExpr));

        const auto& lhsSubscriptExpr = downcast<WGSL::AST::BinaryExpression>(lhsExpr);
        XCTAssert(lhsSubscriptExpr.op() == WGSL::AST::BinaryExpression::Op::Subscript);

        // Check left side of "webkit[0]", which should be "webkit"
        {
            const auto& lhsExpr = lhsSubscriptExpr.lhs();
            XCTAssert(is<WGSL::AST::IdentifierExpression>(lhsExpr));
            const auto& lhsIdent = downcast<WGSL::AST::IdentifierExpression>(lhsExpr);
            XCTAssert(lhsIdent.identifier() == "webkit"_s);
        }

        // Check right side of "webkit[0]", which should be "0"
        {
            const auto& rhsExpr = lhsSubscriptExpr.rhs();
            XCTAssert(is<WGSL::AST::AbstractIntLiteral>(rhsExpr));
            const auto& rhsLiteral = downcast<WGSL::AST::AbstractIntLiteral>(rhsExpr);
            XCTAssert(rhsLiteral.value() == 0);
        }
    }

    // Check right side.
    const auto& rhsIdent = structAccessExpr.fieldName();
    XCTAssert(rhsIdent == "webcore"_s);
}

@end
