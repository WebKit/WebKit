/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "DateTimeFormat.h"

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
#include <gtest/gtest.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

class DateTimeFormatTest : public ::testing::Test {
public:
    typedef DateTimeFormat::FieldType FieldType;

    struct Token {
        String string;
        int count;
        FieldType fieldType;

        Token(FieldType fieldType, int count = 1)
            : count(count)
            , fieldType(fieldType)
        {
            ASSERT(fieldType != DateTimeFormat::FieldTypeLiteral);
        }

        Token(const String& string)
            : string(string)
            , count(0)
            , fieldType(DateTimeFormat::FieldTypeLiteral)
        {
        }

        bool operator==(const Token& other) const
        {
            return fieldType == other.fieldType && count == other.count && string == other.string;
        }

        String toString() const
        {
            switch (fieldType) {
            case DateTimeFormat::FieldTypeInvalid:
                return "*invalid*";
            case DateTimeFormat::FieldTypeLiteral: {
                StringBuilder builder;
                builder.append('"');
                builder.append(string);
                builder.append('"');
                return builder.toString();
            }
            default:
                return String::format("Token(%d, %d)", fieldType, count);
            }
        }
    };

    class Tokens {
    public:
        Tokens() { }

        explicit Tokens(const Vector<Token> tokens)
            : m_tokens(tokens)
        {
        }

        explicit Tokens(const String& string)
        {
            m_tokens.append(Token(string));
        }

        explicit Tokens(Token token1)
        {
            m_tokens.append(token1);
        }

        Tokens(Token token1, Token token2)
        {
            m_tokens.append(token1);
            m_tokens.append(token2);
        }

        Tokens(Token token1, Token token2, Token token3)
        {
            m_tokens.append(token1);
            m_tokens.append(token2);
            m_tokens.append(token3);
        }

        Tokens(Token token1, Token token2, Token token3, Token token4)
        {
            m_tokens.append(token1);
            m_tokens.append(token2);
            m_tokens.append(token3);
            m_tokens.append(token4);
        }

        Tokens(Token token1, Token token2, Token token3, Token token4, Token token5)
        {
            m_tokens.append(token1);
            m_tokens.append(token2);
            m_tokens.append(token3);
            m_tokens.append(token4);
            m_tokens.append(token5);
        }

        Tokens(Token token1, Token token2, Token token3, Token token4, Token token5, Token token6)
        {
            m_tokens.append(token1);
            m_tokens.append(token2);
            m_tokens.append(token3);
            m_tokens.append(token4);
            m_tokens.append(token5);
            m_tokens.append(token6);
        }

        bool operator==(const Tokens& other) const
        {
            return m_tokens == other.m_tokens;
        }

        String toString() const
        {
            StringBuilder builder;
            builder.append("Tokens(");
            for (unsigned index = 0; index < m_tokens.size(); ++index) {
                if (index)
                    builder.append(",");
                builder.append(m_tokens[index].toString());
            }
            builder.append(")");
            return builder.toString();
        }

    private:
        Vector<Token> m_tokens;
    };

protected:
    Tokens parse(const String& formatString)
    {
        TokenHandler handler;
        if (!DateTimeFormat::parse(formatString, handler))
            return Tokens(Token("*failed*"));
        return handler.tokens();
    }

    FieldType single(const char ch)
    {
        char formatString[2];
        formatString[0] = ch;
        formatString[1] = 0;
        TokenHandler handler;
        if (!DateTimeFormat::parse(formatString, handler))
            return DateTimeFormat::FieldTypeInvalid;
        return handler.fieldType(0);
    }

private:
    class TokenHandler : public DateTimeFormat::TokenHandler {
    public:
        virtual ~TokenHandler() { }

        FieldType fieldType(int index) const
        {
            return index >=0 && index < static_cast<int>(m_tokens.size()) ? m_tokens[index].fieldType : DateTimeFormat::FieldTypeInvalid;
        }

        Tokens tokens() const { return Tokens(m_tokens); }

    private:
        virtual void visitField(FieldType fieldType, int count) OVERRIDE
        {
            m_tokens.append(Token(fieldType, count));
        }

        virtual void visitLiteral(const String& string) OVERRIDE
        {
            m_tokens.append(Token(string));
        }

        Vector<Token> m_tokens;
    };
};

std::ostream& operator<<(std::ostream& os, const DateTimeFormatTest::Tokens& tokens)
{
    return os << tokens.toString().ascii().data();
}

TEST_F(DateTimeFormatTest, CommonPattern)
{
    EXPECT_EQ(Tokens(), parse(""));

    EXPECT_EQ(
        Tokens(
            Token(DateTimeFormat::FieldTypeYear, 4), Token("-"),
            Token(DateTimeFormat::FieldTypeMonth, 2), Token("-"),
            Token(DateTimeFormat::FieldTypeDayOfMonth, 2)),
        parse("yyyy-MM-dd"));

    EXPECT_EQ(
        Tokens(
            Token(DateTimeFormat::FieldTypeHour24, 2), Token(":"),
            Token(DateTimeFormat::FieldTypeMinute, 2), Token(":"),
            Token(DateTimeFormat::FieldTypeSecond, 2)),
        parse("kk:mm:ss"));

    EXPECT_EQ(
        Tokens(
            Token(DateTimeFormat::FieldTypeHour12), Token(":"),
            Token(DateTimeFormat::FieldTypeMinute), Token(" "),
            Token(DateTimeFormat::FieldTypePeriod)),
        parse("h:m a"));

    EXPECT_EQ(
        Tokens(
            Token(DateTimeFormat::FieldTypeYear), Token("Nen "),
            Token(DateTimeFormat::FieldTypeMonth), Token("Getsu "),
            Token(DateTimeFormat::FieldTypeDayOfMonth), Token("Nichi")),
        parse("y'Nen' M'Getsu' d'Nichi'"));
}

TEST_F(DateTimeFormatTest, MissingClosingQuote)
{
    EXPECT_EQ(Tokens("*failed*"), parse("'foo"));
    EXPECT_EQ(Tokens("*failed*"), parse("fo'o"));
    EXPECT_EQ(Tokens("*failed*"), parse("foo'"));
}

TEST_F(DateTimeFormatTest, Quote)
{
    EXPECT_EQ(Tokens("FooBar"), parse("'FooBar'"));
    EXPECT_EQ(Tokens("'"), parse("''"));
    EXPECT_EQ(Tokens("'-'"), parse("''-''"));
    EXPECT_EQ(Tokens("Foo'Bar"), parse("'Foo''Bar'"));
    EXPECT_EQ(
        Tokens(Token(DateTimeFormat::FieldTypeEra), Token("'s")),
        parse("G'''s'"));
    EXPECT_EQ(
        Tokens(Token(DateTimeFormat::FieldTypeEra), Token("'"), Token(DateTimeFormat::FieldTypeSecond)),
        parse("G''s"));
}

TEST_F(DateTimeFormatTest, SingleLowerCaseCharacter)
{
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('b'));
    EXPECT_EQ(DateTimeFormat::FieldTypeLocalDayOfWeekStandAlon, single('c'));
    EXPECT_EQ(DateTimeFormat::FieldTypeDayOfMonth, single('d'));
    EXPECT_EQ(DateTimeFormat::FieldTypeLocalDayOfWeek, single('e'));
    EXPECT_EQ(DateTimeFormat::FieldTypeModifiedJulianDay, single('g'));
    EXPECT_EQ(DateTimeFormat::FieldTypeHour12, single('h'));
    EXPECT_EQ(DateTimeFormat::FieldTypeHour24, single('k'));
    EXPECT_EQ(DateTimeFormat::FieldTypeMinute, single('m'));
    EXPECT_EQ(DateTimeFormat::FieldTypeQuaterStandAlone, single('q'));
    EXPECT_EQ(DateTimeFormat::FieldTypeSecond, single('s'));
    EXPECT_EQ(DateTimeFormat::FieldTypeExtendedYear, single('u'));
    EXPECT_EQ(DateTimeFormat::FieldTypeNonLocationZone, single('v'));
    EXPECT_EQ(DateTimeFormat::FieldTypeWeekOfMonth, single('w'));
    EXPECT_EQ(DateTimeFormat::FieldTypeYear, single('y'));
    EXPECT_EQ(DateTimeFormat::FieldTypeZone, single('z'));
}

TEST_F(DateTimeFormatTest, SingleLowerCaseInvalid)
{
    EXPECT_EQ(DateTimeFormat::FieldTypePeriod, single('a'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('f'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('i'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('j'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('l'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('n'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('o'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('p'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('r'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('t'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('x'));
}

TEST_F(DateTimeFormatTest, SingleUpperCaseCharacter)
{
    EXPECT_EQ(DateTimeFormat::FieldTypeMillisecondsInDay, single('A'));
    EXPECT_EQ(DateTimeFormat::FieldTypeDayOfYear, single('D'));
    EXPECT_EQ(DateTimeFormat::FieldTypeDayOfWeek, single('E'));
    EXPECT_EQ(DateTimeFormat::FieldTypeDayOfWeekInMonth, single('F'));
    EXPECT_EQ(DateTimeFormat::FieldTypeEra, single('G'));
    EXPECT_EQ(DateTimeFormat::FieldTypeHour23, single('H'));
    EXPECT_EQ(DateTimeFormat::FieldTypeHour11, single('K'));
    EXPECT_EQ(DateTimeFormat::FieldTypeMonthStandAlone, single('L'));
    EXPECT_EQ(DateTimeFormat::FieldTypeMonth, single('M'));
    EXPECT_EQ(DateTimeFormat::FieldTypeQuater, single('Q'));
    EXPECT_EQ(DateTimeFormat::FieldTypeFractionalSecond, single('S'));
    EXPECT_EQ(DateTimeFormat::FieldTypeWeekOfYear, single('W'));
    EXPECT_EQ(DateTimeFormat::FieldTypeYearOfWeekOfYear, single('Y'));
    EXPECT_EQ(DateTimeFormat::FieldTypeRFC822Zone, single('Z'));
}

TEST_F(DateTimeFormatTest, SingleUpperCaseInvalid)
{
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('B'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('C'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('I'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('J'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('N'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('O'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('P'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('R'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('T'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('U'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('V'));
    EXPECT_EQ(DateTimeFormat::FieldTypeInvalid, single('X'));
}

#endif
