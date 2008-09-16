/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith (catfish.man@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSNthSelector.h"

namespace WebCore {

// a helper function for parsing nth-arguments
bool CSSNthSelector::parseNth()
{
    if (m_parsedNth)
        return true;
    
    const String& m_ar = m_argument;
    
    if (m_ar.isEmpty())
        return false;
    
    m_parsedNth = true;
    m_a = 0;
    m_b = 0;
    if (m_ar == "odd") {
        m_a = 2;
        m_b = 1;
    } else if (m_ar == "even") {
        m_a = 2;
        m_b = 0;
    } else {
        int n = m_ar.find('n');
        if (n != -1) {
            if (m_ar[0] == '-') {
                if (n == 1)
                    m_a = -1; // -n == -1n
                else
                    m_a = m_ar.substring(0, n).toInt();
            } else if (!n)
                m_a = 1; // n == 1n
            else
                m_a = m_ar.substring(0, n).toInt();
            
            int p = m_ar.find('+', n);
            if (p != -1)
                m_b = m_ar.substring(p + 1, m_ar.length() - p - 1).toInt();
            else {
                p = m_ar.find('-', n);
                m_b = -m_ar.substring(p + 1, m_ar.length() - p - 1).toInt();
            }
        } else
            m_b = m_ar.toInt();
    }
    return true;
}

// a helper function for checking nth-arguments
bool CSSNthSelector::matchNth(int count)
{
    if (!m_a)
        return count == m_b;
    else if (m_a > 0) {
        if (count < m_b)
            return false;
        return (count - m_b) % m_a == 0;
    } else {
        if (count > m_b)
            return false;
        return (m_b - count) % (-m_a) == 0;
    }
}

} // namespace WebCore
