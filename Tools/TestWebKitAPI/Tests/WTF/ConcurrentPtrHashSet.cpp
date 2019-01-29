/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/ConcurrentPtrHashSet.h>

namespace TestWebKitAPI {

namespace {

// `commands` is a string that tells this thing what to do. It has to have an even number of characters.
// The character pairs are commands. Commands:
//
// +x     Adds 'x' to the set (casts 'x' to a void*) and asserts that it's new.
// !x     Adds 'x' to the set and asserts that it was there already.
// =x     Asserts that 'x' is in the set
// -x     Asserts that 'x' is not in the set
void doTest(const char* commands)
{
    ConcurrentPtrHashSet set;
    for (const char* command = commands; command[0] && command[1]; command += 2) {
        void* ptr = bitwise_cast<void*>(static_cast<uintptr_t>(command[1]));
        switch (command[0]) {
        case '+':
            EXPECT_TRUE(set.add(ptr));
            break;
        case '!':
            EXPECT_FALSE(set.add(ptr));
            break;
        case '=':
            EXPECT_TRUE(set.contains(ptr));
            break;
        case '-':
            EXPECT_FALSE(set.contains(ptr));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
}

} // anonymous namespace

TEST(WTF_ConcurrentPtrHashSet, Empty)
{
    doTest("");
}

TEST(WTF_ConcurrentPtrHashSet, AddOneElement)
{
    doTest("+x=x-y-0");
}

TEST(WTF_ConcurrentPtrHashSet, AddOneElementMultipleTimes)
{
    doTest("+x!x!x!x!x!x!x!x!x!x!x=x-y-0");
}

TEST(WTF_ConcurrentPtrHashSet, AddOneElementManyTimes)
{
    doTest("+x!x!x!x!x!x!x!x!x!x!x=x-y-0!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x!x");
}

TEST(WTF_ConcurrentPtrHashSet, AddLotsOfElements)
{
    doTest("+1+2+3+4+5+6+7+8+9+0+q+w+e+r+t+y+u+i+o+p+a+s+d+f+g+h+j+k+l+z+x+c+v+b+n+m+Q+W+E+R+T+Y+U+I+O+P+A+S+D+F+G+H+J+K+L+Z+X+C+V+B+N+M=1=2=3=4=5=6=7=8=9=0=q=w=e=r=t=y=u=i=o=p=a=s=d=f=g=h=j=k=l=z=x=c=v=b=n=m=Q=W=E=R=T=Y=U=I=O=P=A=S=D=F=G=H=J=K=L=Z=X=C=V=B=N=M-.");
}

TEST(WTF_ConcurrentPtrHashSet, AddLotsOfElementsManyTimes)
{
    doTest("+1+2+3+4+5+6+7+8+9+0+q+w+e+r+t+y+u+i+o+p+a+s+d+f+g+h+j+k+l+z+x+c+v+b+n+m+Q+W+E+R+T+Y+U+I+O+P+A+S+D+F+G+H+J+K+L+Z+X+C+V+B+N+M=1=2=3=4=5=6=7=8=9=0=q=w=e=r=t=y=u=i=o=p=a=s=d=f=g=h=j=k=l=z=x=c=v=b=n=m=Q=W=E=R=T=Y=U=I=O=P=A=S=D=F=G=H=J=K=L=Z=X=C=V=B=N=M-.!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!y!u!i!o!p!a!s!d!f!g!h!j!k!l!z!x!c!v!b!n!m!Q!W!E!R!T!Y!U!I!O!P!A!S!D!F!G!H!J!K!L!Z!X!C!V!B!N!M!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!y!u!i!o!p!a!s!d!f!g!h!j!k!l!z!x!c!v!b!n!m!Q!W!E!R!T!Y!U!I!O!P!A!S!D!F!G!H!J!K!L!Z!X!C!V!B!N!M!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!y!u!i!o!p!a!s!d!f!g!h!j!k!l!z!x!c!v!b!n!m!Q!W!E!R!T!Y!U!I!O!P!A!S!D!F!G!H!J!K!L!Z!X!C!V!B!N!M!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!y!u!i!o!p!a!s!d!f!g!h!j!k!l!z!x!c!v!b!n!m!Q!W!E!R!T!Y!U!I!O!P!A!S!D!F!G!H!J!K!L!Z!X!C!V!B!N!M!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!y!u!i!o!p!a!s!d!f!g!h!j!k!l!z!x!c!v!b!n!m!Q!W!E!R!T!Y!U!I!O!P!A!S!D!F!G!H!J!K!L!Z!X!C!V!B!N!M!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!y!u!i!o!p!a!s!d!f!g!h!j!k!l!z!x!c!v!b!n!m!Q!W!E!R!T!Y!U!I!O!P!A!S!D!F!G!H!J!K!L!Z!X!C!V!B!N!M!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!y!u!i!o!p!a!s!d!f!g!h!j!k!l!z!x!c!v!b!n!m!Q!W!E!R!T!Y!U!I!O!P!A!S!D!F!G!H!J!K!L!Z!X!C!V!B!N!M!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!y!u!i!o!p!a!s!d!f!g!h!j!k!l!z!x!c!v!b!n!m!Q!W!E!R!T!Y!U!I!O!P!A!S!D!F!G!H!J!K!L!Z!X!C!V!B!N!M!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!y!u!i!o!p!a!s!d!f!g!h!j!k!l!z!x!c!v!b!n!m!Q!W!E!R!T!Y!U!I!O!P!A!S!D!F!G!H!J!K!L!Z!X!C!V!B!N!M");
}

TEST(WTF_ConcurrentPtrHashSet, AddLotsOfElementsAndQuerySomeBeforeAddingTheRest)
{
    doTest("+1+2+3+4+5+6+7+8+9+0+q+w+e+r+t+y+u+i+o+p+a+s+d+f+g+h+j+k=1=2=3=4=5=6=7=8=9=0=q=w=e=r=t=y=u=i=o=p=a=s=d=f=g=h=j=k-l-z-x-c-v-b-n-m-Q-W-E-R-T-Y-U-I-O-P-A-S-D-F-G-H-J-K-L-Z-X-C-V-B-N-M+l+z+x+c+v+b+n+m+Q+W+E+R+T+Y+U+I+O+P+A+S+D+F+G+H+J+K+L+Z+X+C+V+B+N+M=1=2=3=4=5=6=7=8=9=0=q=w=e=r=t=y=u=i=o=p=a=s=d=f=g=h=j=k=l=z=x=c=v=b=n=m=Q=W=E=R=T=Y=U=I=O=P=A=S=D=F=G=H=J=K=L=Z=X=C=V=B=N=M-.");
}

TEST(WTF_ConcurrentPtrHashSet, AddSomeElementsMultipleTimesThenAddTheRestAndQuerySomeBeforeAddingTheRest)
{
    doTest("+1+2+3+4+5+6+7+8+9+0+q+w+e+r+t!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t!1!2!3!4!5!6!7!8!9!0!q!w!e!r!t+y+u+i+o+p+a+s+d+f+g+h+j+k=1=2=3=4=5=6=7=8=9=0=q=w=e=r=t=y=u=i=o=p=a=s=d=f=g=h=j=k-l-z-x-c-v-b-n-m-Q-W-E-R-T-Y-U-I-O-P-A-S-D-F-G-H-J-K-L-Z-X-C-V-B-N-M+l+z+x+c+v+b+n+m+Q+W+E+R+T+Y+U+I+O+P+A+S+D+F+G+H+J+K+L+Z+X+C+V+B+N+M=1=2=3=4=5=6=7=8=9=0=q=w=e=r=t=y=u=i=o=p=a=s=d=f=g=h=j=k=l=z=x=c=v=b=n=m=Q=W=E=R=T=Y=U=I=O=P=A=S=D=F=G=H=J=K=L=Z=X=C=V=B=N=M-.");
}

} // namespace TestWebKitAPI

