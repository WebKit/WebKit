# Copyright (C) 2009 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

use 5.008000;
use Config;
use ExtUtils::MakeMaker;

chomp(my $cc = my $ld = `xcrun -find clang`);

# Remove -arch flags (especially "-arch ppc") from linker flags.
my $archflags = join(" ", map { ("-arch", $_) } split(" ", $ENV{ARCHS})) . " ";
my $config_ldflags = $Config{ldflags};
$config_ldflags =~ s/-arch \S+( |$)//g;
$config_ldflags = $archflags . $config_ldflags;
my $config_lddlflags = $Config{lddlflags};
$config_lddlflags =~ s/-arch \S+( |$)//g;
$config_lddlflags = $archflags . $config_lddlflags;

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'IPhoneSimulatorNotification',
    VERSION_FROM      => 'lib/IPhoneSimulatorNotification.pm', # finds $VERSION
    PREREQ_PM         => {}, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?     ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM  => 'lib/IPhoneSimulatorNotification.pm', # retrieve abstract from module
       AUTHOR         => 'David Kilzer <ddkilzer@apple.com>') : ()),
    LIBS              => [''], # e.g., '-lm'
    CC                => $cc,
    CCFLAGS           => "-x objective-c $archflags",
    LD                => $ld,
    LDDLFLAGS         => $config_lddlflags,
    LDFLAGS           => $config_ldflags,
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I.', # e.g., '-I. -I/usr/include/other'
	# Un-comment this if you add C files to link with later:
    # OBJECT            => '$(O_FILES)', # link all the C files too
);
