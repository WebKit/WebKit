/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)

#include "LocaleICU.h"
#include <gtest/gtest.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

class LocalizedDateICUTest : public ::testing::Test {
public:
    // Labels class is used for printing results in EXPECT_EQ macro.
    class Labels {
    public:
        Labels(const Vector<String> labels)
            : m_labels(labels)
        {
        }

        // FIXME: We should use Vector<T>::operator==() if it works.
        bool operator==(const Labels& other) const
        {
            if (m_labels.size() != other.m_labels.size())
                return false;
            for (unsigned index = 0; index < m_labels.size(); ++index)
                if (m_labels[index] != other.m_labels[index])
                    return false;
            return true;
        }

        String toString() const
        {
            StringBuilder builder;
            builder.append("labels(");
            for (unsigned index = 0; index < m_labels.size(); ++index) {
                if (index)
                    builder.append(", ");
                builder.append('"');
                builder.append(m_labels[index]);
                builder.append('"');
            }
            builder.append(')');
            return builder.toString();
        }

    private:
        Vector<String> m_labels;
    };

protected:
    Labels labels(const String& element1, const String& element2)
    {
        Vector<String> labels = Vector<String>();
        labels.append(element1);
        labels.append(element2);
        return Labels(labels);
    }

    String localizedDateFormatText(const char* localeString)
    {
        OwnPtr<LocaleICU> locale = LocaleICU::create(localeString);
        return locale->timeFormat();
    }

    String localizedShortDateFormatText(const char* localeString)
    {
        OwnPtr<LocaleICU> locale = LocaleICU::create(localeString);
        return locale->shortTimeFormat();
    }

    Labels timeAMPMLabels(const char* localeString)
    {
        OwnPtr<LocaleICU> locale = LocaleICU::create(localeString);
        return Labels(locale->timeAMPMLabels());
    }
};

std::ostream& operator<<(std::ostream& os, const LocalizedDateICUTest::Labels& labels)
{
    return os << labels.toString().utf8().data();
}

TEST_F(LocalizedDateICUTest, localizedDateFormatText)
{
    // Note: EXPECT_EQ(String, String) doesn't print result as string.
    EXPECT_STREQ("h:mm:ss a", localizedDateFormatText("en_US").utf8().data());
    EXPECT_STREQ("HH:mm:ss", localizedDateFormatText("fr").utf8().data());
    EXPECT_STREQ("H:mm:ss", localizedDateFormatText("ja").utf8().data());
}

TEST_F(LocalizedDateICUTest, localizedShortDateFormatText)
{
    EXPECT_STREQ("h:mm a", localizedShortDateFormatText("en_US").utf8().data());
    EXPECT_STREQ("HH:mm", localizedShortDateFormatText("fr").utf8().data());
    EXPECT_STREQ("H:mm", localizedShortDateFormatText("ja").utf8().data());
}

TEST_F(LocalizedDateICUTest, timeAMPMLabels)
{
    EXPECT_EQ(labels("AM", "PM"), timeAMPMLabels("en_US"));
    EXPECT_EQ(labels("AM", "PM"), timeAMPMLabels("fr"));

    UChar jaAM[3] = { 0x5348, 0x524d, 0 };
    UChar jaPM[3] = { 0x5348, 0x5F8C, 0 };
    EXPECT_EQ(labels(String(jaAM), String(jaPM)), timeAMPMLabels("ja"));
}

#endif
