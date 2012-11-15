#!/usr/bin/perl -w

# Copyright (C) 2012 Tony Chang <tony@chromium.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. `AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

use strict;

use InFilesCompiler;

my %defaultParameters = (
);

sub defaultItemFactory
{
    return (
        'conditional' => 0,
        'initial' => '',
        'type' => 'bool'
    );
}

my $InCompiler = InFilesCompiler->new(\%defaultParameters, \&defaultItemFactory);

my $outputDir = $InCompiler->initializeFromCommandLine();
$InCompiler->compile(\&generateCode);

sub generateCode()
{
    my $parsedParametersRef = shift;
    my $parsedItemsRef = shift;

    generateHeader($parsedItemsRef);
}

sub generateHeader($)
{
    my $parsedItemsRef = shift;

    my %parsedItems = %{ $parsedItemsRef };
    my $outputFile = "$outputDir/SettingsMacros.h";

    my %unconditionalSettings = ();
    my %settingsByConditional = ();

    for my $settingName (sort keys %parsedItems) {
        my $conditional = $parsedItems{$settingName}{"conditional"};

        if ($conditional) {
            if (!defined($settingsByConditional{$conditional})) {
                $settingsByConditional{$conditional} = ();
            }
            $settingsByConditional{$conditional}{$settingName} = 1;
        } else {
            $unconditionalSettings{$settingName} = 1;
        }
    }

    open my $file, ">$outputFile" or die "Failed to open file: $!";

    print $file $InCompiler->license();

    # FIXME: Sort by type so bools come last and are bit packed.

    print $file "#ifndef SettingsMacros_h\n";
    print $file "#define SettingsMacros_h\n\n";

    printConditionalMacros($file, \%settingsByConditional, $parsedItemsRef);

    printGettersAndSetters($file, \%unconditionalSettings, \%settingsByConditional, $parsedItemsRef);
    printMemberVariables($file, \%unconditionalSettings, \%settingsByConditional, $parsedItemsRef);
    printInitializerList($file, \%unconditionalSettings, \%settingsByConditional, $parsedItemsRef);

    print $file "#endif // SettingsMacros_h\n";

    close $file;
}

sub printConditionalMacros($$$)
{
    my ($file, $settingsByConditionalRef, $parsedItemsRef) = @_;
    my %parsedItems = %{ $parsedItemsRef };
    my %settingsByConditional = %{ $settingsByConditionalRef };

    for my $conditional (sort keys %settingsByConditional) {
        my $preferredConditional = $InCompiler->preferredConditional($conditional);
        print $file "#if " . $InCompiler->conditionalStringFromAttributeValue($conditional) . "\n";

        print $file "#define ${preferredConditional}_SETTINGS_GETTER_AND_SETTERS \\\n";
        for my $settingName (sort keys %{ $settingsByConditional{$conditional} }) {
            printGetterAndSetter($file, $settingName, $parsedItems{$settingName}{"type"});
        }
        print $file "// End of ${preferredConditional}_SETTINGS_GETTER_AND_SETTERS\n";

        print $file "#define ${preferredConditional}_SETTINGS_NON_BOOL_MEMBER_VARIABLES \\\n";
        for my $settingName (sort keys %{ $settingsByConditional{$conditional} }) {
            my $type = $parsedItems{$settingName}{"type"};
            next if $type eq "bool";
            print $file "    $type m_$settingName; \\\n"
        }
        print $file "// End of ${preferredConditional}_SETTINGS_NON_BOOL_MEMBER_VARIABLES\n";

        print $file "#define ${preferredConditional}_SETTINGS_BOOL_MEMBER_VARIABLES \\\n";
        for my $settingName (sort keys %{ $settingsByConditional{$conditional} }) {
            next if $parsedItems{$settingName}{"type"} ne "bool";
            print $file "    bool m_$settingName : 1; \\\n"
        }
        print $file "// End of ${preferredConditional}_SETTINGS_BOOL_MEMBER_VARIABLES\n";

        print $file "#define ${preferredConditional}_SETTINGS_NON_BOOL_INITIALIZERS \\\n";
        for my $settingName (sort keys %{ $settingsByConditional{$conditional} }) {
            next if $parsedItems{$settingName}{"type"} eq "bool";
            printInitializer($file, $settingName, $parsedItemsRef);
        }
        print $file "// End of ${preferredConditional}_SETTINGS_NON_BOOL_INITIALIZERS\n";

        print $file "#define ${preferredConditional}_SETTINGS_BOOL_INITIALIZERS \\\n";
        for my $settingName (sort keys %{ $settingsByConditional{$conditional} }) {
            next if $parsedItems{$settingName}{"type"} ne "bool";
            printInitializer($file, $settingName, $parsedItemsRef);
        }
        print $file "// End of ${preferredConditional}_SETTINGS_BOOL_INITIALIZERS\n";

        print $file "#else\n";
        print $file "#define ${preferredConditional}_SETTINGS_GETTER_AND_SETTERS\n";
        print $file "#define ${preferredConditional}_SETTINGS_NON_BOOL_MEMBER_VARIABLES\n";
        print $file "#define ${preferredConditional}_SETTINGS_BOOL_MEMBER_VARIABLES\n";
        print $file "#define ${preferredConditional}_SETTINGS_NON_BOOL_INITIALIZERS\n";
        print $file "#define ${preferredConditional}_SETTINGS_BOOL_INITIALIZERS\n";
        print $file "#endif\n";
        print $file "\n";
    }
}

sub printGettersAndSetters($$$$)
{
    my ($file, $unconditionalSettingsRef, $settingsByConditionalRef, $parsedItemsRef) = @_;
    my %parsedItems = %{ $parsedItemsRef };
    my %unconditionalSettings = %{ $unconditionalSettingsRef };
    my %settingsByConditional = %{ $settingsByConditionalRef };

    print $file "#define SETTINGS_GETTERS_AND_SETTERS \\\n";
    for my $settingName (sort keys %unconditionalSettings) {
        printGetterAndSetter($file, $settingName, $parsedItems{$settingName}{"type"});
    }
    for my $conditional (sort keys %settingsByConditional) {
        my $preferredConditional = $InCompiler->preferredConditional($conditional);
        print $file "    ${preferredConditional}_SETTINGS_GETTER_AND_SETTERS \\\n";
    }
    print $file "// End of SETTINGS_GETTERS_AND_SETTERS.\n\n";
}

sub printMemberVariables($$$$)
{
    my ($file, $unconditionalSettingsRef, $settingsByConditionalRef, $parsedItemsRef) = @_;
    my %parsedItems = %{ $parsedItemsRef };
    my %unconditionalSettings = %{ $unconditionalSettingsRef };
    my %settingsByConditional = %{ $settingsByConditionalRef };

    print $file "#define SETTINGS_MEMBER_VARIABLES \\\n";
    # We list the bools last so we can bit pack them.
    for my $settingName (sort keys %unconditionalSettings) {
        my $type = $parsedItems{$settingName}{"type"};
        next if $type eq "bool";
        print $file "    $type m_$settingName; \\\n"
    }
    for my $conditional (sort keys %settingsByConditional) {
        my $preferredConditional = $InCompiler->preferredConditional($conditional);
        print $file "    ${preferredConditional}_SETTINGS_NON_BOOL_MEMBER_VARIABLES \\\n";
    }
    for my $settingName (sort keys %unconditionalSettings) {
        next if $parsedItems{$settingName}{"type"} ne "bool";
        print $file "    bool m_$settingName : 1; \\\n"
    }
    for my $conditional (sort keys %settingsByConditional) {
        my $preferredConditional = $InCompiler->preferredConditional($conditional);
        print $file "    ${preferredConditional}_SETTINGS_BOOL_MEMBER_VARIABLES \\\n";
    }
    print $file "// End of SETTINGS_MEMBER_VARIABLES.\n\n";
}

sub printGetterAndSetter($$$)
{
    my ($file, $settingName, $type) = @_;
    my $setterFunctionName = "set" . $settingName;
    substr($setterFunctionName, 3, 1) = uc(substr($setterFunctionName, 3, 1));
    if (substr($settingName, 0, 3) eq "css" || substr($settingName, 0, 3) eq "xss" || substr($settingName, 0, 3) eq "ftp") {
        substr($setterFunctionName, 3, 3) = uc(substr($setterFunctionName, 3, 3));
    }
    if (lc(substr($type, 0, 1)) eq substr($type, 0, 1)) {
        print $file "    $type $settingName() const { return m_$settingName; } \\\n";
        print $file "    void $setterFunctionName($type $settingName) { m_$settingName = $settingName; } \\\n";
    } else {
        print $file "    const $type& $settingName() { return m_$settingName; } \\\n";
        print $file "    void $setterFunctionName(const $type& $settingName) { m_$settingName = $settingName; } \\\n";
    }
}

sub printInitializerList($$$$)
{
    my ($file, $unconditionalSettingsRef, $settingsByConditionalRef, $parsedItemsRef) = @_;
    my %parsedItems = %{ $parsedItemsRef };
    my %unconditionalSettings = %{ $unconditionalSettingsRef };
    my %settingsByConditional = %{ $settingsByConditionalRef };

    print $file "#define SETTINGS_INITIALIZER_LIST \\\n";
    for my $settingName (sort keys %unconditionalSettings) {
        next if $parsedItems{$settingName}{"type"} eq "bool";
        printInitializer($file, $settingName, $parsedItemsRef);
    }
    for my $conditional (sort keys %settingsByConditional) {
        my $preferredConditional = $InCompiler->preferredConditional($conditional);
        print $file "    ${preferredConditional}_SETTINGS_NON_BOOL_INITIALIZERS \\\n";
    }
    for my $settingName (sort keys %unconditionalSettings) {
        next if $parsedItems{$settingName}{"type"} ne "bool";
        printInitializer($file, $settingName, $parsedItemsRef);
    }
    for my $conditional (sort keys %settingsByConditional) {
        my $preferredConditional = $InCompiler->preferredConditional($conditional);
        print $file "    ${preferredConditional}_SETTINGS_BOOL_INITIALIZERS \\\n";
    }
    print $file "// End of SETTINGS_INITIALIZER_LIST.\n\n";
}

sub printInitializer($$$)
{
    my ($file, $settingName, $parsedItemsRef) = @_;
    my %parsedItems = %{ $parsedItemsRef };

    my $initialValue = $parsedItems{$settingName}{"initial"};
    my $type = $parsedItems{$settingName}{"type"};
    die "Must provide an initial value for $settingName." if ($initialValue eq '' && lc(substr($type, 0, 1)) eq substr($type, 0, 1));
    return if ($initialValue eq '');
    print $file "    , m_$settingName($initialValue) \\\n"
}
