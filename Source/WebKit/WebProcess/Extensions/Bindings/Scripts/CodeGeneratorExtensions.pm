# Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

package CodeGeneratorExtensions;

use strict;
use warnings;
use File::Basename qw(dirname);
use File::Spec;

# Custom types that are reference counted classes, which the implementation receives as a Ref. The IDL names the class, and
# the parser for Ref<Class> does the conversion.
my %refCountedCustomTypes = map { $_ => 1 } qw(
    WebExtensionMatchPattern
);

sub new
{
    my ($class, $codeGenerator, $writeDependencies, $verbose, $idlFilePath) = @_;

    my $reference = {
        codeGenerator => $codeGenerator,
        idlFilePath => $idlFilePath,
    };

    bless($reference, $class);
    return $reference;
}

sub GenerateInterface
{
    my ($self, $interface, $defines, $enumerations, $dictionaries) = @_;

    $$self{dictionaries} = $dictionaries;
    $$self{enumerations} = $enumerations;
}

sub WriteData
{
    my ($self, $interface, $outputDir) = @_;

    foreach my $file ($self->_generateHeaderFile($interface), $self->_generateImplementationFile($interface)) {
        $$self{codeGenerator}->UpdateFile(File::Spec->catfile($outputDir, $$file{name}), join("", @{$$file{contents}}));
    }
}

# The optional keyword is how WebIDL spells an argument that can be left out. The Optional attribute is still read for IDL
# that hasn't moved to the keyword. An optional argument also accepts null and undefined, as other engines' extension
# bindings do, so the generated parser treats both as left out.
sub _isOptionalArgument
{
    my ($argument) = @_;

    return $argument->isOptional || $argument->extendedAttributes->{"Optional"};
}

sub _isMatchableOptionalParameter
{
    my ($self, $parameter) = @_;

    return _isOptionalArgument($parameter) && !$parameter->extendedAttributes->{"CallbackHandler"} && $self->_isDictionaryType($parameter->type);
}

sub _expectedTypeDescription
{
    my ($self, $signature) = @_;

    my $type = $signature->type;

    return "a string" if $$self{codeGenerator}->IsStringType($type);
    return "a boolean" if $type->name eq "boolean";
    return "a number" if $$self{codeGenerator}->IsPrimitiveType($type);
    return "an array" if $type->name eq "array";
    return "a function" if $type->name eq "function";

    return "an object";
}

# The IDL files next to this one, as text, keyed by the interface each declares. A dictionary or enumeration that is not
# declared in this file but in one of them is shared: this interface includes the header of the one that declares it, and
# the one that declares it generates what other interfaces need from it.
sub _siblingInterfaces
{
    my ($self) = @_;

    return $$self{siblingInterfaces} if $$self{siblingInterfaces};

    my %siblings;
    my $ownPath = File::Spec->rel2abs($$self{idlFilePath});

    foreach my $path (glob(File::Spec->catfile(dirname($ownPath), "*.idl"))) {
        next if File::Spec->rel2abs($path) eq $ownPath;

        open(my $handle, "<", $path) or next;
        local $/;
        my $text = <$handle>;
        close($handle);

        my ($interface) = $text =~ /^\s*\]?\s*interface\s+(\w+)\s*(?::\s*\w+\s*)?\{/m;
        $interface = $interface // ($text =~ /\binterface\s+(\w+)\s*(?::\s*\w+\s*)?\{/ ? $1 : undef);
        next unless $interface;

        my %declarations;
        while ($text =~ /^(?:\[([^\]]*)\]\s*)?(dictionary|enum)\s+(\w+)/mg) {
            my ($attributes, $kind, $name) = ($1 // "", $2, $3);
            my ($implementedAs) = $attributes =~ /\bImplementedAs=(\w+)/;

            $declarations{$name} = { kind => $kind, implementedAs => $implementedAs };
        }

        $siblings{$interface} = { text => $text, declarations => \%declarations };
    }

    $$self{siblingInterfaces} = \%siblings;

    return \%siblings;
}

# The interface in a sibling IDL file that declares a dictionary or enumeration with this name, and which of the two it is.
sub _sharedDeclaration
{
    my ($self, $name) = @_;

    my %matches;

    foreach my $interface (sort keys %{$self->_siblingInterfaces}) {
        my $declaration = $self->_siblingInterfaces->{$interface}{declarations}{$name};
        $matches{$interface} = $declaration if $declaration;
    }

    my @owners = sort keys %matches;
    die "'$name' is declared in more than one IDL file: " . join(", ", @owners) . "\n" if scalar(@owners) > 1;

    return undef unless @owners;

    return { owner => $owners[0], kind => $matches{$owners[0]}{kind}, implementedAs => $matches{$owners[0]}{implementedAs} };
}

# Whether another IDL file in the directory mentions the name, so the code for it has to be generated here.
sub _isUsedElsewhere
{
    my ($self, $name) = @_;

    foreach my $interface (keys %{$self->_siblingInterfaces}) {
        my $sibling = $self->_siblingInterfaces->{$interface};
        next if $sibling->{declarations}{$name};

        return 1 if $sibling->{text} =~ /\b\Q$name\E\b/;
    }

    return 0;
}

sub _isDictionaryType
{
    my ($self, $idlType) = @_;

    return 0 unless ref($idlType) eq "IDLType";
    return 0 if $idlType->isUnion;

    return 1 if $$self{codeGenerator}->IsDictionaryType($idlType);

    my $shared = $self->_sharedDeclaration($idlType->name);

    return $shared && $shared->{kind} eq "dictionary" ? 1 : 0;
}

sub _isEnumerationType
{
    my ($self, $idlType) = @_;

    return 0 unless ref($idlType) eq "IDLType";
    return 0 if $idlType->isUnion;

    return 1 if $$self{codeGenerator}->IsEnumType($idlType);

    my $shared = $self->_sharedDeclaration($idlType->name);

    return $shared && $shared->{kind} eq "enum" ? 1 : 0;
}

# Type names with their own handling, which a custom IDL type must not claim.
my %builtInTypeNames = map { $_ => 1 } qw(DOMWindow Promise);

# A custom type is any other capitalized name that no IDL file declares. It is parsed by a WebExtensionValueParser
# specialization, and the generated code includes the header named after it.
sub _isCustomType
{
    my ($self, $idlType) = @_;

    return 0 unless ref($idlType) eq "IDLType";
    return 0 if $idlType->isUnion;

    my $name = $idlType->name;
    return 0 unless $name =~ /^[A-Z]/;
    return 0 if $builtInTypeNames{$name};

    # Interfaces, including ones that Internal adds, are all named WebExtensionAPI.
    return 0 if $name =~ /^WebExtensionAPI/;

    my $codeGenerator = $$self{codeGenerator};
    return 0 if $codeGenerator->IsStringType($idlType) || $codeGenerator->IsPrimitiveType($idlType);
    return 0 if $self->_isEnumerationType($idlType) || $self->_isDictionaryType($idlType);
    return 0 if defined $codeGenerator->IDLFileForInterface($name);

    # The two lists of IDL files don't cover each other, so also look for an interface next to this IDL file.
    return 0 if -e File::Spec->catfile(dirname($$self{idlFilePath}), "${name}.idl");

    return 1;
}

my %numericCppTypes = (
    "double" => "double",
    "unrestricted double" => "double",
    "float" => "double",
    "unrestricted float" => "double",
    "long" => "int32_t",
    "unsigned long" => "uint32_t",
);

# The C++ type a WebExtensionValueParser produces for an IDL type, or undef when the type isn't parsed that way.
sub _parsedCppType
{
    my ($self, $idlType, $description, $customType) = @_;

    return undef unless ref($idlType) eq "IDLType";

    my $codeGenerator = $$self{codeGenerator};

    if ($idlType->isUnion) {
        my @alternatives = map { my $alternative = $_; $self->_parsedCppType($alternative, $description, $customType) // die "Unsupported union member type '" . $alternative->name . "': $description\n" } @{[ @{$idlType->subtypes} ]};
        return "Variant<" . join(", ", @alternatives) . ">";
    }

    my $name = $idlType->name;

    $customType = "Ref<${customType}>" if $customType && $refCountedCustomTypes{$customType};

    die "Unsupported numeric type '$name': $description\n" if $codeGenerator->IsPrimitiveType($idlType) && $name ne "boolean" && !$numericCppTypes{$name};

    # The IDL type says what JavaScript passes, and a custom type is what it is converted to.
    return $customType if $customType && ($codeGenerator->IsStringType($idlType) || $numericCppTypes{$name} || $name eq "any");

    return "String" if $codeGenerator->IsStringType($idlType);
    return "bool" if $name eq "boolean";
    return $numericCppTypes{$name} if $numericCppTypes{$name};
    return "Protected<JSValueRef>" if $name eq "any";

    if ($codeGenerator->IsSequenceType($idlType)) {
        my $elementIDLType = $idlType->subtypes->[0];
        die "A sequence needs an element type: $description\n" unless defined $elementIDLType;

        my $elementType = $self->_parsedCppType($elementIDLType, $description, $customType) // die "Unsupported sequence element type '" . $elementIDLType->name . "': $description\n";
        return "Vector<${elementType}>";
    }

    if ($name eq "record") {
        my ($keyType, $valueType) = @{$idlType->subtypes};
        die "A record needs a DOMString key: $description\n" unless $codeGenerator->IsStringType($keyType);

        my $valueCppType = $self->_parsedCppType($valueType, $description, $customType) // die "Unsupported record value type '" . $valueType->name . "': $description\n";
        return "HashMap<String, ${valueCppType}>";
    }

    return $self->_cppTypeName($name) if $self->_isEnumerationType($idlType) || $self->_isDictionaryType($idlType);

    return $name if $self->_isCustomType($idlType);

    return undef;
}

# Whether an operation argument is parsed by a WebExtensionValueParser, which covers the types that aren't a plain string,
# number, boolean, any or function.
sub _isParsedArgumentType
{
    my ($self, $idlType, $signature) = @_;

    return 0 unless ref($idlType) eq "IDLType";
    return 1 if $signature && $signature->extendedAttributes->{"CustomType"};

    my $codeGenerator = $$self{codeGenerator};

    return 1 if $idlType->isUnion;
    return 1 if $codeGenerator->IsSequenceType($idlType) || $idlType->name eq "record";
    return 1 if $self->_isEnumerationType($idlType) || $self->_isDictionaryType($idlType) || $self->_isCustomType($idlType);

    return 0;
}

# The type that holds the result of parsing an argument, which is empty when an optional argument is missing.
sub _parsedArgumentResultType
{
    my ($self, $idlType, $isOptional, $description, $customType) = @_;

    my $cppType = $self->_parsedCppType($idlType, $description, $customType);

    return $isOptional ? "std::expected<std::optional<${cppType}>, WebExtensionValueError>" : "std::expected<${cppType}, WebExtensionValueError>";
}

# What generated C++ calls a dictionary, an enumeration or a member, which an ImplementedAs attribute can set apart from the
# name in the IDL. The name in the IDL is the one JavaScript sees.
sub _cppTypeName
{
    my ($self, $name) = @_;

    my ($dictionary) = grep { $_->type->name eq $name } @{$$self{dictionaries}};
    return $dictionary->extendedAttributes->{"ImplementedAs"} // $name if $dictionary;

    my ($enumeration) = grep { $_->type->name eq $name } @{$$self{enumerations}};
    return $enumeration->extendedAttributes->{"ImplementedAs"} // $name if $enumeration;

    my $shared = $self->_sharedDeclaration($name);

    return $shared && $shared->{implementedAs} ? $shared->{implementedAs} : $name;
}

sub _cppEnumerationName
{
    my ($self, $enumeration) = @_;

    return $enumeration->extendedAttributes->{"ImplementedAs"} // $enumeration->type->name;
}

sub _cppDictionaryName
{
    my ($self, $dictionary) = @_;

    return $dictionary->extendedAttributes->{"ImplementedAs"} // $dictionary->type->name;
}

sub _cppMemberName
{
    my ($self, $member) = @_;

    return $member->extendedAttributes->{"ImplementedAs"} // $member->name;
}

sub _dictionaryByName
{
    my ($self, $name) = @_;

    my ($dictionary) = grep { $_->type->name eq $name } @{$$self{dictionaries}};

    return $dictionary;
}

# A dictionary's members, starting with the ones it inherits.
sub _dictionaryMembers
{
    my ($self, $dictionary) = @_;

    my @members = ();

    if (my $parentType = $dictionary->parentType) {
        my $parent = $self->_dictionaryByName($parentType->name);
        die "Dictionary '" . $parentType->name . "' must be declared in the same IDL file as '" . $dictionary->type->name . "'\n" unless $parent;

        push(@members, $self->_dictionaryMembers($parent));
    }

    push(@members, @{$dictionary->members});

    return @members;
}

# Records the dictionaries, enumerations and custom types that an IDL type refers to.
sub _collectTypeDependencies
{
    my ($self, $idlType, $dependencies) = @_;

    return unless ref($idlType) eq "IDLType";

    if ($idlType->isUnion || $$self{codeGenerator}->IsSequenceType($idlType) || $idlType->name eq "record") {
        # A named variable, because callees that read files overwrite $_ and would clear an aliased element.
        foreach my $subtype (@{$idlType->subtypes}) {
            $self->_collectTypeDependencies($subtype, $dependencies);
        }

        return;
    }

    my $name = $idlType->name;

    if ($self->_isDictionaryType($idlType)) {
        push(@{$$dependencies{dictionaries}}, $name);
    } elsif ($self->_isEnumerationType($idlType)) {
        push(@{$$dependencies{enumerations}}, $name);
    } elsif ($self->_isCustomType($idlType)) {
        push(@{$$dependencies{customTypes}}, $name);
    }
}

# What the generated code needs for an interface's parsed arguments: the dictionaries (ordered so a dictionary comes
# after the ones its members use), the enumerations, and the custom types.
sub _parsedTypeDependencies
{
    my ($self, $interface) = @_;

    my %cached = %{$$self{parsedTypeDependencies}{$interface->type->name} || {}};
    return \%cached if %cached;

    my @dictionaries = ();
    my @enumerations = ();
    my @customTypes = ();
    my %seen;

    my $addUnique = sub {
        my ($list, $kind, $name) = @_;

        return if $seen{"$kind:$name"};
        $seen{"$kind:$name"} = 1;

        push(@$list, $name);
    };

    # Dictionaries that another interface declares, which this interface includes the header of instead of declaring again.
    my @importedFrom = ();

    my $visitDictionary;
    my $visitDependencies = sub {
        my ($dependencies) = @_;

        foreach my $enumerationName (@{$$dependencies{enumerations} || []}) {
            if (grep { $_->type->name eq $enumerationName } @{$$self{enumerations}}) {
                $addUnique->(\@enumerations, "enumeration", $enumerationName);
                next;
            }

            my $shared = $self->_sharedDeclaration($enumerationName) or die "Enumeration '$enumerationName' is not declared in this IDL file or one next to it\n";
            push(@importedFrom, $shared->{owner}) unless grep { $_ eq $shared->{owner} } @importedFrom;
        }
        $addUnique->(\@customTypes, "custom", $_) foreach @{$$dependencies{customTypes} || []};
        $visitDictionary->($_) foreach @{$$dependencies{dictionaries} || []};
    };

    $visitDictionary = sub {
        my ($name) = @_;

        return if $seen{"dictionary:$name"};
        $seen{"dictionary:$name"} = 1;

        my $dictionary = $self->_dictionaryByName($name);

        unless ($dictionary) {
            my $shared = $self->_sharedDeclaration($name) or die "Dictionary '$name' is not declared in this IDL file or one next to it\n";

            push(@importedFrom, $shared->{owner}) unless grep { $_ eq $shared->{owner} } @importedFrom;
            return;
        }

        foreach my $member ($self->_dictionaryMembers($dictionary)) {
            my %dependencies;
            $self->_collectTypeDependencies($member->type, \%dependencies);
            push(@{$dependencies{customTypes}}, $member->extendedAttributes->{"CustomType"}) if $member->extendedAttributes->{"CustomType"};
            $visitDependencies->(\%dependencies);
        }

        push(@dictionaries, $name);
    };

    foreach my $function (@{$interface->operations}) {
        foreach my $argument (@{$function->arguments}) {
            next unless $self->_isParsedArgumentType($argument->type, $argument);

            my %dependencies;
            $self->_collectTypeDependencies($argument->type, \%dependencies);
            push(@{$dependencies{customTypes}}, $argument->extendedAttributes->{"CustomType"}) if $argument->extendedAttributes->{"CustomType"};
            $visitDependencies->(\%dependencies);
        }
    }

    # What another interface in the directory uses from this one is generated here, with the code to parse it. Only an
    # interface that parses arguments includes the parser header that the code needs.
    my $parsesArguments = grep { $self->_isParsedArgumentType($_->type, $_) } map { @{$_->arguments} } @{$interface->operations};

    foreach my $dictionary (@{$$self{dictionaries}}) {
        my $name = $dictionary->type->name;
        next unless $parsesArguments && $self->_isUsedElsewhere($name);

        $visitDictionary->($name);
    }

    foreach my $enumeration (@{$$self{enumerations}}) {
        my $name = $enumeration->type->name;
        next unless $parsesArguments && $self->_isUsedElsewhere($name);

        $visitDependencies->({ enumerations => [ $name ] });
    }

    foreach my $owner (@importedFrom) {
        my $ownerText = $self->_siblingInterfaces->{$owner}{text};
        foreach my $name (map { $_->type->name } @{$$self{dictionaries}}, @{$$self{enumerations}}) {
            die "The interfaces " . $interface->type->name . " and $owner use each other's dictionaries or enumerations ('$name'), which includes each header from the other\n" if $ownerText =~ /\b\Q$name\E\b/;
        }
    }

    my $result = {
        dictionaries => [ map { $self->_dictionaryByName($_) } @dictionaries ],
        enumerations => [ map { my $name = $_; my ($enumeration) = grep { $_->type->name eq $name } @{$$self{enumerations}}; die "Enumeration '$name' must be declared in the same IDL file as the interface\n" unless $enumeration; $enumeration } @enumerations ],
        customTypes => \@customTypes,
        importedFrom => \@importedFrom,
    };

    $$self{parsedTypeDependencies}{$interface->type->name} = $result;

    return $result;
}

sub _usedDictionaries
{
    my ($self, $interface) = @_;

    return @{$self->_parsedTypeDependencies($interface)->{dictionaries}};
}

sub _usesParsedTypes
{
    my ($self, $interface) = @_;

    foreach my $function (@{$interface->operations}) {
        return 1 if grep { $self->_isParsedArgumentType($_->type, $_) } @{$function->arguments};
    }

    return 0;
}

# Members of kind function are the only ones that aren't read through a WebExtensionValueParser.
sub _isFunctionMember
{
    my ($self, $member) = @_;

    return $member->type->name eq "function" ? 1 : 0;
}

# The C++ type of a member. Required members are the bare type, optional ones wrap it in std::optional, a nullable
# member adds a second std::optional for null, and a member with a default is the bare type.
sub _dictionaryMemberBaseType
{
    my ($self, $dictionary, $member) = @_;

    my $description = $dictionary->type->name . "." . $member->name;

    return "Protected<JSValueRef>" if $self->_isFunctionMember($member);

    return $self->_parsedCppType($member->type, $description, $member->extendedAttributes->{"CustomType"}) // die "Unsupported dictionary member type '" . $member->type->name . "': $description\n";
}

sub _dictionaryMemberType
{
    my ($self, $dictionary, $member) = @_;

    my $baseType = $self->_dictionaryMemberBaseType($dictionary, $member);
    my $isNullable = $member->type->isNullable;

    return $baseType if defined $member->default;
    return $isNullable ? "std::optional<${baseType}>" : $baseType if $member->isRequired;
    return $isNullable ? "std::optional<std::optional<${baseType}>>" : "std::optional<${baseType}>";
}

sub _enumerationValueName
{
    my ($self, $value) = @_;

    # A value that mixes cases, like syntaxError, keeps its capitals.
    my $keepsCase = $value =~ /[a-z]/ && $value =~ /[A-Z]/;

    return join("", map { $keepsCase ? ucfirst($_) : ucfirst(lc($_)) } grep { length($_) } split(/[_\-\s]+/, $value));
}

# The object an API exposes for an enumeration, like windows.WindowState. Each value is a property named in upper case with
# underscores, such as NO_RESTRICTION, whose value is what the API returns for it.
sub _enumerationObjectExpression
{
    my ($self, $enumerationName) = @_;

    my ($enumeration) = grep { $_->type->name eq $enumerationName } @{$$self{enumerations}};
    die "EnumObject names unknown enumeration '$enumerationName'\n" unless $enumeration;

    my @entries = map {
        my $key = $_;
        $key =~ s/([a-z])([A-Z])/$1_$2/g;
        $key =~ s/-/_/g;
        $key = "_$key" if $key =~ /^[0-9]/;

        "{ \"" . uc($key) . "\"_s, \"" . $_ . "\"_s }"
    } @{$enumeration->values};

    return "makeEnumObject(context, { " . join(", ", @entries) . " })";
}

sub _enumerationValuesName
{
    my ($self, $enumeration) = @_;

    return CodeGenerator::WK_lcfirst(undef, $self->_cppEnumerationName($enumeration)) . "Values";
}

sub _enumerationDeclaration
{
    my ($self, $enumeration) = @_;

    my $name = $self->_cppEnumerationName($enumeration);

    # An enumeration marked ExistingType is declared elsewhere, such as in Shared, with the same enumerators in the same
    # order, so it isn't declared again.
    my $result = "";

    unless ($enumeration->extendedAttributes->{"ExistingType"}) {
        $result = "enum class ${name} : uint8_t {\n";
        foreach my $value (@{$enumeration->values}) {
            $result .= "    " . $self->_enumerationValueName($value) . ",\n";
        }
        $result .= "};\n\n";
    }

    $result .= <<EOF;
template<> struct WebExtensionValueParser<${name}> : WebExtensionStringParserBase {
    static std::expected<${name}, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

EOF

    return $result;
}

sub _enumerationImplementation
{
    my ($self, $enumeration) = @_;

    my $name = $self->_cppEnumerationName($enumeration);
    my $valuesName = $self->_enumerationValuesName($enumeration);
    my @values = map { "\"" . lc($_) . "\"_s" } @{$enumeration->values};

    # The parser converts the index of the matching value, so an existing enumeration has to list its enumerators in the
    # order that the IDL does.
    my $orderChecks = "";
    if ($enumeration->extendedAttributes->{"ExistingType"}) {
        foreach my $index (0 .. $#{$enumeration->values}) {
            my $valueName = $self->_enumerationValueName($enumeration->values->[$index]);
            $orderChecks .= "static_assert(static_cast<size_t>(${name}::${valueName}) == ${index}, \"${name}::${valueName} has to be enumerator ${index}\");\n";
        }

        $orderChecks .= "\n";
    }

    return $orderChecks . <<EOF;
static constexpr std::array<ASCIILiteral, @{[scalar(@values)]}> ${valuesName} { @{[join(", ", @values)]} };

std::expected<${name}, WebExtensionValueError> WebExtensionValueParser<${name}>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto index = parseWebExtensionEnumValue(context, value, path, ${valuesName});
    if (!index)
        return makeUnexpected(WTF::move(index.error()));

    return static_cast<${name}>(*index);
}

EOF
}

# The wrapper that lets a generated dictionary be used as a member, sequence element or union alternative.
sub _dictionaryParserSpecialization
{
    my ($self, $dictionary) = @_;

    my $name = $self->_cppDictionaryName($dictionary);

    return <<EOF;
template<> struct WebExtensionValueParser<${name}> : WebExtensionObjectParserBase {
    static std::expected<${name}, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path) { return parse${name}(context, value, path); }
};

EOF
}

sub _conditionalBlock
{
    my ($self, $member, $text) = @_;

    my $conditionalString = conditionalString($member);

    return $text unless $conditionalString;
    return "#if ${conditionalString}\n${text}#endif\n";
}

sub _dictionaryDeclaration
{
    my ($self, $dictionary) = @_;

    my $name = $self->_cppDictionaryName($dictionary);

    my $result = "struct ${name} {\n";
    foreach my $member ($self->_dictionaryMembers($dictionary)) {
        $result .= $self->_conditionalBlock($member, "    " . $self->_dictionaryMemberType($dictionary, $member) . " " . $self->_cppMemberName($member) . ";\n");
    }
    $result .= "};\n\n";
    $result .= "std::expected<${name}, WebExtensionValueError> parse${name}(JSContextRef, JSValueRef, const String& path = { });\n\n";
    $result .= $self->_dictionaryParserSpecialization($dictionary);

    return $result;
}

sub _dictionaryDeclarations
{
    my ($self, $interface) = @_;

    my $dependencies = $self->_parsedTypeDependencies($interface);

    return join("", map { $self->_enumerationDeclaration($_) } @{$$dependencies{enumerations}}) . join("", map { $self->_dictionaryDeclaration($_) } @{$$dependencies{dictionaries}});
}

sub _dictionaryImplementations
{
    my ($self, $interface) = @_;

    my $dependencies = $self->_parsedTypeDependencies($interface);

    return join("", map { $self->_enumerationImplementation($_) } @{$$dependencies{enumerations}}) . join("", map { $self->_dictionaryImplementation($_) } @{$$dependencies{dictionaries}});
}

# The C++ for a default value in the IDL, for a member of the given type.
sub _defaultValueExpression
{
    my ($self, $dictionary, $member) = @_;

    my $default = $member->default;
    my $type = $member->type;
    my $description = $dictionary->type->name . "." . $member->name;

    die "A default value on a nullable member isn't supported: $description\n" if $type->isNullable;

    return $default if $type->name eq "boolean" && $default =~ /^(true|false)$/;
    return $default if $numericCppTypes{$type->name} && $default =~ /^-?[0-9.]+$/;

    if ($default =~ /^"(.*)"$/) {
        my $value = $1;

        return "String { \"${value}\"_s }" if $$self{codeGenerator}->IsStringType($type);
        return $self->_cppTypeName($type->name) . "::" . $self->_enumerationValueName($value) if $self->_isEnumerationType($type);
    }

    return "{ }" if $default eq "[]" && $$self{codeGenerator}->IsSequenceType($type);

    die "Unsupported default value '$default': $description\n";
}

sub _dictionaryImplementation
{
    my ($self, $dictionary) = @_;

    my $name = $self->_cppDictionaryName($dictionary);
    my @members = $self->_dictionaryMembers($dictionary);
    my %membersByName = map { $_->name => $_ } @members;
    my $hasRequiredMembers = scalar(grep { $_->isRequired && !defined $_->default } @members) ? 1 : 0;

    my $result = <<EOF;
std::expected<${name}, WebExtensionValueError> parse${name}(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);
EOF

    $result .= "    Vector<String> missingRequiredKeys;\n" if $hasRequiredMembers;
    $result .= "\n";

    foreach my $member (@members) {
        my $memberName = $member->name;
        my $reading = $self->_dictionaryMemberReading($dictionary, $member, $memberName, $memberName, "${memberName}Value");

        if (my $alias = $member->extendedAttributes->{"Alias"}) {
            die "Alias on " . $dictionary->type->name . ".$memberName can't name an existing member\n" if $membersByName{$alias};

            $reading .= $self->_dictionaryMemberReading($dictionary, $member, $alias, $alias, "${memberName}AliasValue");
        }

        if ($member->isRequired && !defined $member->default) {
            $reading .= <<EOF;
    if (!${memberName}Value)
        missingRequiredKeys.append("${memberName}"_s);

EOF
        }

        $result .= $self->_conditionalBlock($member, $reading);
    }

    # Type errors above win over missing required keys.
    $result .= <<EOF if $hasRequiredMembers;
    if (!missingRequiredKeys.isEmpty())
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString("it is missing required keys: "_s, formatList(missingRequiredKeys)) });

EOF

    $result .= $self->_dictionaryMemberChecks($dictionary);

    my @initializers = map {
        my $member = $_;
        my $variable = $member->name . "Value";

        my $expression = "WTF::move(${variable})";
        $expression = "${variable}.value_or(" . $self->_defaultValueExpression($dictionary, $member) . ")" if defined $member->default;
        $expression = "WTF::move(*${variable})" if $member->isRequired && !defined $member->default;

        [ $member, ".@{[$self->_cppMemberName($member)]} = ${expression}" ];
    } @members;

    if (grep { conditionalString($_->[0]) } @initializers) {
        $result .= "    return ${name} {\n";
        $result .= $self->_conditionalBlock($_->[0], "        $_->[1],\n") foreach @initializers;
        $result .= "    };\n";
    } else {
        $result .= "    return ${name} { " . join(", ", map { $_->[1] } @initializers) . " };\n";
    }

    $result .= "}\n\n";

    return $result;
}

# Generates the statements that read one key into a std::optional named $variable, for a dictionary member.
sub _dictionaryMemberReading
{
    my ($self, $dictionary, $member, $key, $variablePrefix, $variable) = @_;

    my $baseType = $self->_dictionaryMemberBaseType($dictionary, $member);

    if ($self->_isFunctionMember($member)) {
        return <<EOF;
    auto ${variablePrefix}Read = readDictionaryFunctionMember(context, object, "${key}"_s, path);
    if (!${variablePrefix}Read)
        return makeUnexpected(WTF::move(${variablePrefix}Read.error()));

    std::optional<${baseType}> ${variable} = WTF::move(*${variablePrefix}Read);

EOF
    }

    my $isNullable = $member->type->isNullable;
    my $reader = $isNullable ? "readDictionaryNullableMember" : ($member->extendedAttributes->{"PreserveNull"} ? "readDictionaryMemberKeepingNull" : "readDictionaryMember");
    my $localType = $isNullable ? "std::optional<std::optional<${baseType}>>" : "std::optional<${baseType}>";

    return <<EOF;
    auto ${variablePrefix}Read = ${reader}<${baseType}>(context, object, "${key}"_s, path);
    if (!${variablePrefix}Read)
        return makeUnexpected(WTF::move(${variablePrefix}Read.error()));

    ${localType} ${variable} = WTF::move(*${variablePrefix}Read);

EOF
}

# Checks that depend on more than one member, or on the contents of a member. The wording matches the existing
# messages in the API implementations, and the error names the dictionary when it is nested. These checks are for
# extension developers: the UI process validates its own IPC input and never relies on them.
sub _dictionaryMemberChecks
{
    my ($self, $dictionary) = @_;

    my $dictionaryName = $dictionary->type->name;
    my @members = $self->_dictionaryMembers($dictionary);
    my %membersByName = map { $_->name => $_ } @members;
    my $result = "";

    # A member counts as specified when it is present. A nullable member counts only when it isn't null, and a boolean
    # marked FalseIsDefault only when it is true, since null and false ask for the default.
    my $specified = sub {
        my ($name) = @_;

        my $member = $membersByName{$name};
        my $isFalseDefault = $member->extendedAttributes->{"FalseIsDefault"};

        if ($isFalseDefault) {
            die "FalseIsDefault on $dictionaryName.$name needs a boolean member\n" unless $self->_dictionaryMemberBaseType($dictionary, $member) eq "bool" && !$member->type->isNullable;

            return "${name}Value.value_or(false)";
        }

        return "(${name}Value && *${name}Value)" if $member->type->isNullable && !$member->isRequired;

        return "${name}Value";
    };

    my $checkNames = sub {
        my ($attributeName, $names) = @_;

        die "$attributeName on $dictionaryName needs a list of at least two members, like (first, second)\n" unless ref($names) eq "ARRAY" && scalar(@$names) >= 2;

        foreach my $name (@$names) {
            my $member = $membersByName{$name};

            die "$attributeName on $dictionaryName names unknown member '$name'\n" unless defined $member;
            die "$attributeName on $dictionaryName names required member '$name'\n" if $member->isRequired && !defined $member->default;
            die "$attributeName on $dictionaryName names conditional member '$name'\n" if conditionalString($member);
        }
    };

    # A member given by its legacy alias is merged into the member, and giving both is an error.
    foreach my $member (@members) {
        my $alias = $member->extendedAttributes->{"Alias"};
        next unless $alias;

        my $name = $member->name;

        $result .= $self->_conditionalBlock($member, <<EOF);
    if (${name}Value && ${name}AliasValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it cannot specify both '${name}' and '${alias}'. Please use '${name}'"_s });

    if (!${name}Value)
        ${name}Value = WTF::move(${name}AliasValue);

EOF
    }

    # Members that name the same AtMostOneOf group conflict, like radio buttons that share a name. A member can be in
    # more than one group by naming a list of them. The message names the pair in declaration order.
    my (@groupNames, %groupMembers);
    foreach my $member (@members) {
        my $groups = $member->extendedAttributes->{"AtMostOneOf"};
        next unless $groups;

        foreach my $group (ref($groups) eq "ARRAY" ? @$groups : $groups) {
            push(@groupNames, $group) unless $groupMembers{$group};
            push(@{$groupMembers{$group}}, $member->name);
        }
    }

    foreach my $group (@groupNames) {
        my $names = $groupMembers{$group};

        die "AtMostOneOf group '$group' on $dictionaryName needs at least two members\n" unless scalar(@$names) >= 2;

        $checkNames->("AtMostOneOf", $names);

        # One check for each pair, so the message names the two members that were both specified.
        foreach my $firstIndex (0 .. $#$names - 1) {
            foreach my $secondIndex ($firstIndex + 1 .. $#$names) {
                my ($first, $second) = ($names->[$firstIndex], $names->[$secondIndex]);
                my ($firstCondition, $secondCondition) = ($specified->($first), $specified->($second));

                $result .= <<EOF;
    if (${firstCondition} && ${secondCondition})
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it cannot specify both '${first}' and '${second}'"_s });

EOF
            }
        }
    }

    if (my $names = $dictionary->extendedAttributes->{"AtLeastOneOf"}) {
        $checkNames->("AtLeastOneOf", $names);

        my $condition = join(" && ", map { "!" . $specified->($_) } @$names);
        my $choices = $self->_choicesText(@$names);

        $result .= <<EOF;
    if (${condition})
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it must specify either ${choices}"_s });

EOF
    }

    foreach my $member (@members) {
        my $required = $member->extendedAttributes->{"RequiresMember"};
        next unless $required;

        my $name = $member->name;

        die "RequiresMember on $dictionaryName.$name names unknown member '$required'\n" unless $membersByName{$required};

        my ($condition, $requiredCondition) = ($specified->($name), $specified->($required));

        $result .= $self->_conditionalBlock($member, <<EOF);
    if (${condition} && !${requiredCondition})
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it must specify both '${required}' and '${name}'"_s });

EOF
    }

    foreach my $member (@members) {
        my $name = $member->name;
        my $isScalarNumber = $numericCppTypes{$member->type->name} && !$member->type->isNullable;

        my $checks = "";

        if ($member->extendedAttributes->{"CannotBeEmpty"}) {
            $checks .= <<EOF;
    if (${name}Value && ${name}Value->isEmpty())
        return makeUnexpected(WebExtensionValueError { "${name}"_s, "it must not be empty"_s });

EOF
        }

        if (my $maximumLength = $member->extendedAttributes->{"MaxLength"}) {
            $checks .= <<EOF;
    if (${name}Value && makeUTF8CString(*${name}Value).length() > ${maximumLength})
        return makeUnexpected(WebExtensionValueError { "${name}"_s, "it exceeded the maximum allowed length"_s });

EOF
        }

        my ($minimum, $maximum) = ($member->extendedAttributes->{"Minimum"}, $member->extendedAttributes->{"Maximum"});

        if (defined $minimum || defined $maximum) {
            die "Minimum and Maximum on $dictionaryName.$name need a number member\n" unless $isScalarNumber;

            my @conditions = ();
            push(@conditions, "*${name}Value < ${minimum}") if defined $minimum;
            push(@conditions, "*${name}Value > ${maximum}") if defined $maximum;

            my $message = "makeString(\"it must be at least \"_s, ${minimum})" if defined $minimum && !defined $maximum;
            $message = "\"it must not be negative\"_s" if defined $minimum && $minimum eq "0" && !defined $maximum;
            $message = "makeString(\"it must be at most \"_s, ${maximum})" if !defined $minimum && defined $maximum;
            $message = "makeString(\"it must specify a value between \"_s, ${minimum}, \" and \"_s, ${maximum})" if defined $minimum && defined $maximum;

            $checks .= <<EOF;
    if (${name}Value && (@{[join(" || ", @conditions)]}))
        return makeUnexpected(WebExtensionValueError { "${name}"_s, ${message} });

EOF
        }

        $result .= $self->_conditionalBlock($member, $checks) if $checks;
    }

    return $result;
}

# Formats "'a'", "'a' or 'b'", or "'a', 'b', or 'c'" for a list of names.
sub _choicesText
{
    my ($self, @names) = @_;

    my @quoted = map { "'$_'" } @names;
    return $quoted[0] if scalar(@quoted) == 1;
    return "$quoted[0] or $quoted[1]" if scalar(@quoted) == 2;

    my $last = pop(@quoted);
    return join(", ", @quoted) . ", or $last";
}

sub _className
{
    my ($idlType) = @_;

    return "JS" . _implementationClassName($idlType);
}

sub _classRefGetter
{
    my ($self, $idlType, $suffix) = @_;

    $suffix = "Class" unless defined $suffix;

    my $implementationClassName = _implementationClassName($idlType);
    $implementationClassName =~ s/^WebExtensionAPI//;

    return $$self{codeGenerator}->WK_lcfirst($implementationClassName) . $suffix . "Singleton";
}

sub _parseLicenseBlock
{
    my ($fileHandle) = @_;

    my ($copyright, $readCount, $buffer, $currentCharacter, $previousCharacter);
    my $startSentinel = "/*";
    my $lengthOfStartSentinel = length($startSentinel);
    $readCount = read($fileHandle, $buffer, $lengthOfStartSentinel);
    return "" if ($readCount < $lengthOfStartSentinel || $buffer ne $startSentinel);
    $copyright = $buffer;

    while ($readCount = read($fileHandle, $currentCharacter, 1)) {
        $copyright .= $currentCharacter;
        return $copyright if $currentCharacter eq "/" && $previousCharacter eq "*";
        $previousCharacter = $currentCharacter;
    }

    return "";
}

sub _parseLicenseBlockFromFile
{
    my ($path) = @_;
    open my $fileHandle, "<", $path or die "Failed to open $path for reading: $!";
    my $licenseBlock = _parseLicenseBlock($fileHandle);
    close($fileHandle);
    return $licenseBlock;
}

sub _defaultLicenseBlock
{
    return <<EOF;
/*
 * Copyright (C) 2020-@{[_yearString()]} Apple Inc. All rights reserved.
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
EOF
}

sub _licenseBlock
{
    my ($self) = @_;
    return $self->{licenseBlock} if $self->{licenseBlock};

    my $licenseBlock = _parseLicenseBlockFromFile($self->{idlFilePath}) || _defaultLicenseBlock();
    $self->{licenseBlock} = $licenseBlock;
    return $licenseBlock;
}

sub conditionalString
{
    my ($node) = @_;
    my $conditional = $node->extendedAttributes->{"Conditional"};
    return "" unless $conditional;
    # We really should be calling this as $$self{codeGenerator}->Generate..., but that makes it hard
    # to call this function from ExtensionsGlobalObjectsCollector.pl. Since
    # GenerateConditionalStringFromAttributeValue doesn't actually use the object reference, this
    # way works just fine.
    return CodeGenerator::GenerateConditionalStringFromAttributeValue(0, $conditional);
}

sub platformString
{
    my ($node) = @_;
    my $platform = $node->extendedAttributes->{"Platform"};
    return "" unless $platform;
    return "PLATFORM($platform)";
}

sub _generateHeaderFile
{
    my ($self, $interface) = @_;

    my @contents = ();

    push(@contents, $self->_licenseBlock());
    push(@contents, "\n\n");

    push(@contents, "#pragma once\n\n");

    my $idlType = $interface->type;
    my $className = _className($idlType);
    my $implementationClassName = _implementationClassName($idlType);
    my $parentClassName = $self->_parentClassName($interface);
    my $filename = $className . ".h";

    my $conditionalString = conditionalString($interface);
    push(@contents, <<EOF) if $conditionalString;
#if ${conditionalString}

EOF

    my $dictionaryDeclarations = $self->_dictionaryDeclarations($interface);

    push(@contents, <<EOF);
#include "${parentClassName}.h"
EOF

    push(@contents, "#include \"JSWebExtensionValueParser.h\"\n") if $self->_usesParsedTypes($interface);
    push(@contents, map { "#include \"JS${_}.h\"\n" } @{$self->_parsedTypeDependencies($interface)->{importedFrom}});
    # Custom types are declared by the parser header, which includes what each one needs, and an existing enumeration
    # comes from its own header.
    push(@contents, map { "#include \"" . $self->_cppEnumerationName($_) . ".h\"\n" } grep { $_->extendedAttributes->{"ExistingType"} } @{$self->_parsedTypeDependencies($interface)->{enumerations}});

    push(@contents, "#include \"${className}Custom.h\"\n") if $interface->extendedAttributes->{"CustomHeader"};
    push(@contents, <<EOF);

namespace WebKit {

using namespace WTF;

${dictionaryDeclarations}class ${implementationClassName};

class ${className} : public ${parentClassName} {
public:
    static JSClassRef @{[$self->_classRefGetter($idlType)]}();
    static JSClassRef @{[$self->_classRefGetter($idlType, "GlobalObjectClass")]}();

private:
    static const JSStaticFunction* staticFunctions();
    static const JSStaticValue* staticValues();
    static const JSStaticValue* staticConstants();
    static bool hasInstance(JSContextRef, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception);
EOF

    my $hasDynamicProperties = 0;
    my $hasMainWorldOnlyProperties = 0;

    if (my @functions = @{$interface->operations}) {
        push(@contents, "\n    // Functions\n\n");
        push(@contents, "    static void getPropertyNames(JSContextRef, JSObjectRef, JSPropertyNameAccumulatorRef);\n") if $interface->extendedAttributes->{"CustomGetPropertyNames"};
        push(@contents, "    static bool hasProperty(JSContextRef, JSObjectRef, JSStringRef);\n") if $interface->extendedAttributes->{"CustomHasProperty"};

        foreach my $function (@functions) {
            $hasDynamicProperties = 1 if $function->extendedAttributes->{"Dynamic"};
            $hasMainWorldOnlyProperties = 1 if $function->extendedAttributes->{"MainWorldOnly"};

            my $conditionalString = conditionalString($function);
            my $platformString = platformString($function);

            push(@contents, "#if ${platformString}\n") if $platformString;
            push(@contents, "#if ${conditionalString}\n") if $conditionalString;
            push(@contents, "    static JSValueRef @{[$function->name]}(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);\n");
            push(@contents, "    static bool setProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, JSValueRef*);\n") if $function->extendedAttributes->{"SetProperty"};
            push(@contents, "    static JSValueRef getProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n") if $function->extendedAttributes->{"GetProperty"};
            push(@contents, "    static bool deleteProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n") if $function->extendedAttributes->{"DeleteProperty"};
            push(@contents, "#endif // ${conditionalString}\n") if $conditionalString;
            push(@contents, "#endif // ${platformString}\n") if $platformString;
        }
    }

    if (my @constants = @{$interface->constants}) {
        push(@contents, "\n    // Constants\n\n");
        foreach my $constant (@constants) {
            my $getterName = $constant->name;
            push(@contents, "    static JSValueRef ${getterName}(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n");
        }
    }

    if (my @attributes = @{$interface->attributes}) {
        push(@contents, "\n    // Attributes\n\n");
        foreach my $attribute (@attributes) {
            next if $attribute->extendedAttributes->{"NoImplementation"};

            $hasDynamicProperties = 1 if $attribute->extendedAttributes->{"Dynamic"};
            $hasMainWorldOnlyProperties = 1 if $attribute->extendedAttributes->{"MainWorldOnly"};

            my $conditionalString = conditionalString($attribute);
            my $platformString = platformString($attribute);

            push(@contents, "#if ${platformString}\n") if $platformString;
            push(@contents, "#if ${conditionalString}\n") if $conditionalString;
            push(@contents, "    static JSValueRef @{[$self->_getterName($attribute)]}(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n");
            push(@contents, "    static bool @{[$self->_setterName($attribute)]}(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, JSValueRef*);\n") unless $attribute->isReadOnly;
            push(@contents, "#endif // ${conditionalString}\n") if $conditionalString;
            push(@contents, "#endif // ${platformString}\n") if $platformString;
        }
    }

    if ($hasDynamicProperties or $hasMainWorldOnlyProperties) {
        push(@contents, "\n    // Dynamic Attributes\n\n");
        push(@contents, "    static void getPropertyNames(JSContextRef, JSObjectRef, JSPropertyNameAccumulatorRef);\n");
        push(@contents, "    static bool hasProperty(JSContextRef, JSObjectRef, JSStringRef);\n");
        push(@contents, "    static JSValueRef getProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n");
    }

    push(@contents, <<EOF);
};

${implementationClassName}* to${implementationClassName}(JSContextRef, JSValueRef);

} // namespace WebKit

EOF

    push(@contents, <<EOF) if $conditionalString;
#endif // ${conditionalString}

EOF

    push(@contents, <<EOF);
EOF

    return { name => $filename, contents => \@contents };
}

sub _generateImplementationFile
{
    my ($self, $interface) = @_;

    my @contentsPrefix = ();
    my %contentsIncludes = ();
    my @contents = ();

    my $idlType = $interface->type;
    my $className = _className($idlType);
    my $implementationClassName = _implementationClassName($idlType);
    my $filename = $interface->extendedAttributes->{"UseCPPAPI"} ? $className . ".cpp" : $className . ".mm";

    push(@contentsPrefix, $self->_licenseBlock());
    push(@contentsPrefix, "\n\n");

    if (!$interface->extendedAttributes->{"UseCPPAPI"})
    {
        push(@contentsPrefix, <<EOF);
#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

EOF
    }

    push(@contentsPrefix, "#include \"config.h\"\n\n");

    my $conditionalString = conditionalString($interface);
    push(@contentsPrefix, <<EOF) if $conditionalString;
#if ${conditionalString}

EOF

    my $classRefGetter = $self->_classRefGetter($idlType);
    my $globalClassRefGetter = $self->_classRefGetter($idlType, "GlobalObjectClass");
    my $parentClassName = $self->_parentClassName($interface);

    $contentsIncludes{"\"${className}.h\""} = 1;
    $contentsIncludes{"\"${implementationClassName}.h\""} = 1;
    $contentsIncludes{"\"Logging.h\""} = 1;
    $contentsIncludes{"\"WebExtensionUtilities.h\""} = 1;
    $contentsIncludes{"\"WebPage.h\""} = 1;
    $contentsIncludes{"<wtf/GetPtr.h>"} = 1;

    push(@contents, <<EOF);

namespace WebKit {

${implementationClassName}* to${implementationClassName}(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !${className}::${classRefGetter}() || !JSValueIsObjectOfClass(context, value, ${className}::${classRefGetter}()))
        return nullptr;
    return downcast<${implementationClassName}>(JSWebExtensionWrapper::unwrap(context, value));
}

@{[$self->_dictionaryImplementations($interface)]}JSClassRef ${className}::${classRefGetter}()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "@{[_publicClassName($idlType)]}";
        definition.parentClass = @{[$self->_parentClassRefGetterExpression($interface)]};
        definition.staticValues = staticValues();
        definition.staticFunctions = staticFunctions();
EOF

    push(@contents, "        definition.initialize = initialize;\n") unless _parentInterface($interface);
    push(@contents, "        definition.finalize = finalize;\n") unless _parentInterface($interface);
    push(@contents, "        definition.getPropertyNames = getPropertyNames;\n") if $interface->extendedAttributes->{"CustomGetPropertyNames"};
    push(@contents, "        definition.hasProperty = hasProperty;\n") if $interface->extendedAttributes->{"CustomHasProperty"};

    my $hasDynamicProperties = 0;
    my $hasMainWorldOnlyProperties = 0;

    if (my @functions = @{$interface->operations}) {
        foreach my $function (@functions) {
            $hasDynamicProperties = 1 if $function->extendedAttributes->{"Dynamic"};
            $hasMainWorldOnlyProperties = 1 if $function->extendedAttributes->{"MainWorldOnly"};

            push(@contents, "        definition.setProperty = setProperty;\n") if $function->extendedAttributes->{"SetProperty"};
            push(@contents, "        definition.getProperty = getProperty;\n") if $function->extendedAttributes->{"GetProperty"};
            push(@contents, "        definition.deleteProperty = deleteProperty;\n") if $function->extendedAttributes->{"DeleteProperty"};
        }
    }

    if (my @attributes = @{$interface->attributes}) {
        foreach my $attribute (@attributes) {
            $hasDynamicProperties = 1 if $attribute->extendedAttributes->{"Dynamic"};
            $hasMainWorldOnlyProperties = 1 if $attribute->extendedAttributes->{"MainWorldOnly"};
        }
    }

    if ($hasDynamicProperties or $hasMainWorldOnlyProperties) {
        push(@contents, "        definition.getPropertyNames = getPropertyNames;\n");
        push(@contents, "        definition.hasProperty = hasProperty;\n");
        push(@contents, "        definition.getProperty = getProperty;\n");
    }

    push(@contents, <<EOF);
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

EOF
    push (@contents, <<EOF);
JSClassRef ${className}::${globalClassRefGetter}()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "@{[_publicClassName($idlType)]}";
        definition.staticValues = staticConstants();
        definition.hasInstance = hasInstance;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

EOF

    push(@contents, $self->_staticFunctionsGetterImplementation($interface), "\n");
    push(@contents, $self->_staticValuesGetterImplementation($interface), "\n");
    push(@contents, $self->_staticConstantsGetterImplementation($interface), "\n");
    push(@contents, $self->_hasInstanceImplementation($interface));
    push(@contents, $self->_dynamicAttributesImplementation($interface, $hasDynamicProperties, $hasMainWorldOnlyProperties));

    my $earlyReturnCondition = "!impl";
    $earlyReturnCondition .= " || !impl->isForMainWorld()" if $interface->extendedAttributes->{"MainWorldOnly"};

    if (my @functions = @{$interface->operations}) {
        push(@contents, "\n// Functions\n");

        if (my $customGetPropertyNamesFunction = $interface->extendedAttributes->{"CustomGetPropertyNames"}) {
            push(@contents, <<EOF);

void ${className}::getPropertyNames(JSContextRef context, JSObjectRef thisObject, JSPropertyNameAccumulatorRef propertyNames)
{
    RefPtr impl = to${implementationClassName}(context, thisObject);
    if (!impl) [[unlikely]]
        return;

    NSArray *propertyNameStrings = impl->${customGetPropertyNamesFunction}();

    for (NSString *propertyName in propertyNameStrings)
        JSPropertyNameAccumulatorAddName(propertyNames, toJSString(propertyName).get());
}
EOF
        }

        if (my $customHasPropertyFunction = $interface->extendedAttributes->{"CustomHasProperty"}) {
            push(@contents, <<EOF);

bool ${className}::hasProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName)
{
    RefPtr impl = to${implementationClassName}(context, thisObject);
    if (!impl) [[unlikely]]
        return false;

    return impl->${customHasPropertyFunction}(toString(propertyName));
}
EOF
        }

        foreach my $function (@functions) {
            next if $function->extendedAttributes->{"Custom"};

            my $functionEarlyReturnCondition = $earlyReturnCondition;

            push(@contents, "\n");

            my $functionSignature = "JSValueRef ${className}::@{[$function->name]}(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)";
            my $call = _callString($idlType, $function, 1);

            my $defaultEarlyReturnValue = "JSValueMakeUndefined(context)";
            my $defaultReturnValue = $defaultEarlyReturnValue;

            my $isSetPropertyFunction = defined $function->extendedAttributes->{"SetProperty"};
            my $isGetPropertyFunction = defined $function->extendedAttributes->{"GetProperty"};
            my $isDeletePropertyFunction = defined $function->extendedAttributes->{"DeleteProperty"};
            my $isPropertyFunction = $isSetPropertyFunction || $isGetPropertyFunction || $isDeletePropertyFunction;

            my $conditionalString = conditionalString($function);
            push(@contents, <<EOF) if $conditionalString;
#if ${conditionalString}
EOF

            if ($isSetPropertyFunction) {
                $defaultEarlyReturnValue = "false";
                $defaultReturnValue = "true";
                push(@contents, "bool ${className}::setProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef newValue, JSValueRef* exception)");
            } elsif ($isGetPropertyFunction) {
                push(@contents, "JSValueRef ${className}::getProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)");
            } elsif ($isDeletePropertyFunction) {
                $defaultEarlyReturnValue = "false";
                $defaultReturnValue = "true";
                push(@contents, "bool ${className}::deleteProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)");
            } else {
                push(@contents, $functionSignature);
            }

            push(@contents, <<EOF);

{
    RefPtr impl = to${implementationClassName}(context, thisObject);
    if (${functionEarlyReturnCondition}) [[unlikely]]
        return ${defaultEarlyReturnValue};

    RELEASE_LOG_DEBUG(Extensions, "Called function ${call} (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));
EOF

            my @parameters = ();
            my @specifiedParameters = @{$function->arguments};
            my $needsNSExceptionString = $function->extendedAttributes->{"RaisesException"};
            my $needsExceptionString = $function->extendedAttributes->{"RaisesStringException"};

            my $lastParameter = $specifiedParameters[$#specifiedParameters];

            push(@contents, "\n    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);\n\n") if scalar @specifiedParameters;
            my $needsScriptContext = $function->extendedAttributes->{"NeedsScriptContext"};

            my $needsFrame = $function->extendedAttributes->{"NeedsFrame"};
            my $needsFrameIdentifier = $function->extendedAttributes->{"NeedsFrameIdentifier"};
            my $needsPage = $function->extendedAttributes->{"NeedsPage"};
            my $needsPageIdentifier = $function->extendedAttributes->{"NeedsPageIdentifier"};
            my $returnsPromiseIfNoCallback = $function->extendedAttributes->{"ReturnsPromiseWhenCallbackIsOmitted"} || $interface->extendedAttributes->{"ReturnsPromiseWhenCallbackIsOmitted"};
            my $callbackHandlerArgument;

            $self->_includeHeaders(\%contentsIncludes, $function->type, $function);

            my $argumentCount = scalar @specifiedParameters;
            my $optionalArgumentCount = 0;

            foreach my $parameter (@specifiedParameters) {
                $self->_includeHeaders(\%contentsIncludes, $parameter->type, $parameter);
                $optionalArgumentCount++ if _isOptionalArgument($parameter);
                $needsPage = 1 if $parameter->extendedAttributes->{"CallbackHandler"} && $interface->extendedAttributes->{"NeedsPageWithCallbackHandler"};
                $needsPageIdentifier = 1 if $parameter->extendedAttributes->{"CallbackHandler"} && $interface->extendedAttributes->{"NeedsPageIdentifierWithCallbackHandler"};
                $needsFrame = 1 if $parameter->extendedAttributes->{"CallbackHandler"} && $interface->extendedAttributes->{"NeedsFrameWithCallbackHandler"};
                $needsFrameIdentifier = 1 if $parameter->extendedAttributes->{"CallbackHandler"} && $interface->extendedAttributes->{"NeedsFrameIdentifierWithCallbackHandler"};
                $callbackHandlerArgument = $parameter->name if $parameter->extendedAttributes->{"CallbackHandler"} && _isOptionalArgument($parameter);
            }

            my $processArgumentsLeftToRight = $function->extendedAttributes->{"ProcessArgumentsLeftToRight"};
            my $requiredArgumentCount = $argumentCount - $optionalArgumentCount;
            my $hasOptionalAsLastArgument = 1 if $lastParameter && _isOptionalArgument($lastParameter);
            my $hasSimpleOptionalArgumentHandling = !$optionalArgumentCount || ($hasOptionalAsLastArgument && ($argumentCount <= 2 || $requiredArgumentCount >= $argumentCount - 1));
            my $argumentIndexConditon = undef;

            if (!$hasSimpleOptionalArgumentHandling) {
                push(@contents, "    ssize_t argumentIndex = -1;\n") unless $processArgumentsLeftToRight;
                push(@contents, "    size_t argumentIndex = argumentCount;\n") if $processArgumentsLeftToRight;

                foreach my $parameter (grep { $self->_isMatchableOptionalParameter($_) } @specifiedParameters) {
                    push(@contents, "    bool " . $parameter->name . "Matched = false;\n");
                }
            }

            if ($requiredArgumentCount) {
                push(@contents, <<EOF);
    constexpr size_t requiredArgumentCount = ${requiredArgumentCount};
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, nullString(), "a required argument is missing"_s);
        return ${defaultEarlyReturnValue};
    }

EOF
            }

            if ($optionalArgumentCount) {
                die "Property functions can't take optional arguments" if $isPropertyFunction;

                foreach my $parameter (@specifiedParameters) {
                    push(@contents, "    " . $self->_platformTypeVariableDeclaration($interface, $parameter, $parameter->name) . "\n");
                    push(@parameters, $parameter->name . ".releaseNonNull()") if $parameter->extendedAttributes->{"CallbackHandler"} && _isOptionalArgument($parameter);
                    push(@parameters, "*" . $parameter->name) if $parameter->extendedAttributes->{"Serialization"} && $parameter->extendedAttributes->{"Serialization"} eq "Message";
                    push(@parameters, "*" . $parameter->name) if $self->_isParsedArgumentType($parameter->type, $parameter);
                    push(@parameters, $parameter->name . ".createNSString().get()") if $parameter->type->name eq "DOMString" && !$parameter->extendedAttributes->{"URL"} && !$interface->extendedAttributes->{"UseCPPAPI"};
                    push(@parameters, $parameter->name) unless $self->_isParsedArgumentType($parameter->type, $parameter) || ($parameter->extendedAttributes->{"Serialization"} && $parameter->extendedAttributes->{"Serialization"} eq "Message") || ($parameter->extendedAttributes->{"CallbackHandler"} && _isOptionalArgument($parameter)) || ($parameter->type->name eq "DOMString" && !$parameter->extendedAttributes->{"URL"} && !$interface->extendedAttributes->{"UseCPPAPI"});
                }

                push(@contents, "\n");

                my $hasOptionalCallbackHandlerAsLastArgument = 1 if $lastParameter->extendedAttributes->{"CallbackHandler"} && $hasOptionalAsLastArgument;

                push(@contents, "    if (argumentCount == ${argumentCount}) {\n");

                foreach my $i (0..$#specifiedParameters) {
                    my $parameter = $specifiedParameters[$i];
                    $self->_installArgumentTypeExceptions(\@contents, $parameter, $idlType, "arguments[${i}]", $parameter->name, $defaultEarlyReturnValue, \%contentsIncludes, $function, 1, 2);
                }

                foreach my $i (0..$#specifiedParameters) {
                    my $parameter = $specifiedParameters[$i];
                    push(@contents, "        " . $self->_platformTypeVariableDeclaration($interface, $parameter, $parameter->name, "arguments[${i}]", undef, 1) . "\n");
                }

                if ($hasSimpleOptionalArgumentHandling && $argumentCount > 1) {
                    push(@contents, "    } else if (argumentCount == " . ($argumentCount - 1) . ") {\n") if $requiredArgumentCount eq ($argumentCount - 1) || !$hasOptionalAsLastArgument;
                    push(@contents, "    } else if (argumentCount == " . ($argumentCount - 1) . " && !(" . $self->_javaScriptTypeCondition($lastParameter, "arguments[0]") . ")) {\n") if $hasOptionalAsLastArgument && $requiredArgumentCount ne ($argumentCount - 1);

                    foreach my $i (0..$#specifiedParameters - 1) {
                        my $parameter = $specifiedParameters[$i];
                        $self->_installArgumentTypeExceptions(\@contents, $parameter, $idlType, "arguments[${i}]", $parameter->name, $defaultEarlyReturnValue, \%contentsIncludes, $function, 1, 2);
                    }

                    foreach my $i (0..$#specifiedParameters - 1) {
                        my $parameter = $specifiedParameters[$i];
                        push(@contents, "        " . $self->_platformTypeVariableDeclaration($interface, $parameter, $parameter->name, "arguments[${i}]", undef, 1) . "\n");
                    }

                    if ($hasOptionalAsLastArgument && $requiredArgumentCount ne ($argumentCount - 1)) {
                        push(@contents, "    } else if (argumentCount == 1) {\n");
                        $self->_installArgumentTypeExceptions(\@contents, $lastParameter, $idlType, "arguments[0]", $lastParameter->name, $defaultEarlyReturnValue, \%contentsIncludes, $function, 1, 2);
                        push(@contents, "        " . $self->_platformTypeVariableDeclaration($interface, $lastParameter, $lastParameter->name, "arguments[0]", undef, 1) . "\n");
                    }
                } elsif ($argumentCount > 1) {
                    push(@contents, "    } else {\n");

                    push(@contents, "        JSValueRef currentArgument = nullptr;\n");

                    push(@contents, "        size_t allowedOptionalArgumentCount = argumentCount - requiredArgumentCount;\n") if $requiredArgumentCount;
                    push(@contents, "        size_t processedOptionalArgumentCount = 0;\n") if $requiredArgumentCount;
                    push(@contents, "        argumentIndex = argumentCount - 1;\n") unless $processArgumentsLeftToRight;
                    push(@contents, "        argumentIndex = 0;\n") if $processArgumentsLeftToRight;

                    if ($hasOptionalCallbackHandlerAsLastArgument && $processArgumentsLeftToRight) {
                        my $parameter = $specifiedParameters[$#specifiedParameters];
                        my $optionalCondition = $requiredArgumentCount ? "allowedOptionalArgumentCount && " : "";
                        push(@contents, "\n");
                        push(@contents, "        if ($optionalCondition(currentArgument = arguments[argumentCount - 1]) && (" . $self->_javaScriptTypeCondition($parameter, "currentArgument") . ")) {\n");
                        push(@contents, "            " . $self->_platformTypeVariableDeclaration($interface, $parameter, $parameter->name, "currentArgument", undef, 1) . "\n");
                        push(@contents, "            --allowedOptionalArgumentCount;\n") if $requiredArgumentCount;
                        push(@contents, "            if (argumentCount)\n");
                        push(@contents, "                --argumentCount;\n");
                        push(@contents, "        }\n");
                    }

                    $argumentIndexConditon = $processArgumentsLeftToRight ? "argumentIndex < argumentCount" : "argumentIndex >= 0";
                    my $nextArgumentIndex = $processArgumentsLeftToRight ? "++argumentIndex" : "--argumentIndex";

                    for (my $j = $processArgumentsLeftToRight ? 0 : $#specifiedParameters; $processArgumentsLeftToRight ? $j <= $#specifiedParameters : $j >= 0; $processArgumentsLeftToRight ? $j++ : $j--) {
                        my $parameter = $specifiedParameters[$j];
                        my $lastArgument = $j eq ($processArgumentsLeftToRight ? $#specifiedParameters : 0);

                        next if $lastArgument && $hasOptionalCallbackHandlerAsLastArgument && $processArgumentsLeftToRight;

                        push(@contents, "\n");

                        my $optionalCondition = $requiredArgumentCount && _isOptionalArgument($parameter) ? " && processedOptionalArgumentCount < allowedOptionalArgumentCount" : "";
                        push(@contents, "        if ($argumentIndexConditon$optionalCondition && (currentArgument = arguments[argumentIndex])) {\n");

                        if (_isOptionalArgument($parameter)) {
                            my $matchCondition = $self->_javaScriptTypeCondition($parameter, "currentArgument");

                            # Arguments are matched from the right, so a value that is not an object would be skipped and then matched
                            # against an earlier argument, which names the wrong one. An optional dictionary also takes a value that sits
                            # at its own position, and parsing it reports the mismatch.
                            if (!$processArgumentsLeftToRight && $self->_isDictionaryType($parameter->type)) {
                                $matchCondition = "(${matchCondition}) || argumentIndex == ${j}";
                            }

                            push(@contents, "            if (${matchCondition}) {\n");
                            push(@contents, "                " . $self->_platformTypeVariableDeclaration($interface, $parameter, $parameter->name, "currentArgument", undef, 1) . "\n");
                            push(@contents, "                ++processedOptionalArgumentCount;\n") if $requiredArgumentCount;
                            push(@contents, "                " . $parameter->name . "Matched = true;\n") if $self->_isMatchableOptionalParameter($parameter);
                            push(@contents, "                $nextArgumentIndex;\n");
                            push(@contents, "            }\n");
                        } else {
                            $self->_installArgumentTypeExceptions(\@contents, $parameter, $idlType, "currentArgument", $parameter->name, $defaultEarlyReturnValue, \%contentsIncludes, $function, 1, 3);
                            push(@contents, "            " . $self->_platformTypeVariableDeclaration($interface, $parameter, $parameter->name, "currentArgument", undef, 1) . "\n");
                            push(@contents, "            $nextArgumentIndex;\n");
                        }

                        push(@contents, "        }\n");
                    }
                }

                push(@contents, "    }\n");
            } else {
                foreach my $i (0..$#specifiedParameters) {
                    my $parameter = $specifiedParameters[$i];
                    $self->_installArgumentTypeExceptions(\@contents, $parameter, $idlType, "arguments[${i}]", $parameter->name, $defaultEarlyReturnValue, \%contentsIncludes, $function, 1, 1);
                }

                foreach my $i (0..$#specifiedParameters) {
                    my $parameter = $specifiedParameters[$i];

                    if ($isPropertyFunction && $i == 0) {
                        push(@contents, "    NSString *" . $parameter->name . " = toString(propertyName).createNSString().get();\n");
                    } else {
                        my $argument = $isSetPropertyFunction ? "newValue" : "arguments[$i]";

                        die "GetProperty and DeleteProperty functions only take one argument" if $isGetPropertyFunction || $isDeletePropertyFunction;

                        push(@contents, "    " . $self->_platformTypeVariableDeclaration($interface, $parameter, $parameter->name, $argument) . "\n");
                    }

                    push(@parameters, $parameter->name . ".releaseNonNull()") if $parameter->extendedAttributes->{"CallbackHandler"} && _isOptionalArgument($parameter);
                    push(@parameters, "*" . $parameter->name) if $parameter->extendedAttributes->{"Serialization"} && $parameter->extendedAttributes->{"Serialization"} eq "Message";
                    push(@parameters, "*" . $parameter->name) if $self->_isParsedArgumentType($parameter->type, $parameter);
                    push(@parameters, $parameter->name . ".createNSString().get()") if $parameter->type->name eq "DOMString" && !$parameter->extendedAttributes->{"URL"} && !$interface->extendedAttributes->{"UseCPPAPI"};
                    push(@parameters, $parameter->name) unless $self->_isParsedArgumentType($parameter->type, $parameter) || ($parameter->extendedAttributes->{"Serialization"} && $parameter->extendedAttributes->{"Serialization"} eq "Message") || ($parameter->extendedAttributes->{"CallbackHandler"} && _isOptionalArgument($parameter)) || ($parameter->type->name eq "DOMString" && !$parameter->extendedAttributes->{"URL"} && !$interface->extendedAttributes->{"UseCPPAPI"});
                }
            }

            my $mapFunction = sub {
                return $_->extendedAttributes->{"ImplementedAs"} if $_->extendedAttributes->{"ImplementedAs"};
                return $_->name;
            };

            my @methodSignatureNames = map(&$mapFunction, @specifiedParameters);

            foreach my $parameter (@specifiedParameters) {
                $self->_installAutomaticExceptions(\@contents, $interface, $parameter, $idlType, $parameter->name, $parameter->name, $defaultEarlyReturnValue, \%contentsIncludes, $function, 1);
            }

            if (!$hasSimpleOptionalArgumentHandling) {
                push(@contents, "\n");
                push(@contents, "    if ($argumentIndexConditon) [[unlikely]] {\n");

                # Blame the parameter at the unmatched argument's position when it did not match, otherwise the first
                # optional parameter that did not match.
                my @matchable = grep { $self->_isMatchableOptionalParameter($_) } @specifiedParameters;

                foreach my $position (0..$#specifiedParameters) {
                    my $parameter = $specifiedParameters[$position];
                    next unless $self->_isMatchableOptionalParameter($parameter);

                    my $name = $parameter->name;
                    my $expected = $self->_expectedTypeDescription($parameter);

                    push(@contents, <<EOF);
        if (argumentIndex == ${position} && !${name}Matched) {
            *exception = toJSError(context, "${call}"_s, "${name}"_s, makeString("${expected} is expected, but "_s, describeValueType(context, arguments[argumentIndex]), " was provided"_s));
            return ${defaultEarlyReturnValue};
        }

EOF
                }

                foreach my $parameter (@matchable) {
                    my $name = $parameter->name;
                    my $expected = $self->_expectedTypeDescription($parameter);

                    push(@contents, <<EOF);
        if (!${name}Matched) {
            *exception = toJSError(context, "${call}"_s, "${name}"_s, makeString("${expected} is expected, but "_s, describeValueType(context, arguments[argumentIndex]), " was provided"_s));
            return ${defaultEarlyReturnValue};
        }

EOF
                }

                push(@contents, "        *exception = toJSError(context, \"${call}\"_s, nullString(), \"an unknown argument was provided\"_s);\n");
                push(@contents, "        return ${defaultEarlyReturnValue};\n");
                push(@contents, "    }\n");
            }

            unshift(@methodSignatureNames, "context") if $needsScriptContext;
            unshift(@parameters, "context") if $needsScriptContext;

            unshift(@methodSignatureNames, "frame") if $needsFrame;
            unshift(@parameters, "*frame") if $needsFrame;

            unshift(@methodSignatureNames, "frameIdentifier") if $needsFrameIdentifier;
            unshift(@parameters, "frame->frameID()") if $needsFrameIdentifier;

            unshift(@methodSignatureNames, "page") if $needsPage;
            unshift(@parameters, "*page") if $needsPage;

            unshift(@methodSignatureNames, "webPageProxyIdentifier") if $needsPageIdentifier;
            unshift(@parameters, "page->webPageProxyIdentifier()") if $needsPageIdentifier;

            push(@methodSignatureNames, "outExceptionString") if $needsNSExceptionString;
            push(@parameters, "&exceptionString") if $needsNSExceptionString;

            push(@methodSignatureNames, "outExceptionString") if $needsExceptionString;
            push(@parameters, "exceptionString") if $needsExceptionString;

            my $isVoidReturn = $function->type->name eq "void";
            my $functionCall = $self->_functionCall($function, \@methodSignatureNames, \@parameters, $interface);
            my $returnExpression = $self->_returnExpression($function, $functionCall, $interface);

            push(@contents, "\n");

            my $returnsPromise = $callbackHandlerArgument && $returnsPromiseIfNoCallback;
            if ($returnsPromise) {
                die "Returning a Promise is only allowed for void functions" unless $isVoidReturn;

                $defaultReturnValue = "promiseResult ?: $defaultReturnValue";

                push(@contents, <<EOF);
    JSObjectRef promiseResult = nullptr;
    if (!${callbackHandlerArgument}) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        ${callbackHandlerArgument} = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

EOF
            }

            if ($callbackHandlerArgument && !$returnsPromiseIfNoCallback) {
                push(@contents, <<EOF);
    if (!${callbackHandlerArgument})
        ${callbackHandlerArgument} = toJSErrorCallbackHandler(context, protect(impl->runtime()));

EOF
            }

            if ($needsPage || $needsPageIdentifier) {
                push(@contents, "    RefPtr page = toWebPage(context);\n");
                push(@contents, "    if (!page) [[unlikely]] {\n");
                push(@contents, "        RELEASE_LOG_ERROR(Extensions, \"Page could not be found for JSContextRef\");\n");
                push(@contents, "        if (promiseResult)\n") if $returnsPromise;
                push(@contents, "            promiseResult = toJSRejectedPromise(context, \"${call}\"_s, nullString(), \"an unknown error occurred\"_s);\n") if $returnsPromise;
                push(@contents, "        return ${defaultReturnValue};\n");
                push(@contents, "    }\n\n");
            }

            if ($needsFrame || $needsFrameIdentifier) {
                push(@contents, "    RefPtr frame = toWebFrame(context);\n");
                push(@contents, "    if (!frame) [[unlikely]] {\n");
                push(@contents, "        RELEASE_LOG_ERROR(Extensions, \"Frame could not be found for JSContextRef\");\n");
                push(@contents, "        if (promiseResult)\n") if $returnsPromise;
                push(@contents, "            promiseResult = toJSRejectedPromise(context, \"${call}\"_s, nullString(), \"an unknown error occurred\"_s);\n") if $returnsPromise;
                push(@contents, "        return ${defaultReturnValue};\n");
                push(@contents, "    }\n\n");
            }

            if ($needsNSExceptionString && !$isVoidReturn) {
                push(@contents, <<EOF);
    NSString *exceptionString;
    JSValueRef result = ${returnExpression};

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, nullString(), String(exceptionString));
        return ${defaultEarlyReturnValue};
    }

    return result;
}
EOF
            } elsif ($needsNSExceptionString && $isVoidReturn) {
                push(@contents, <<EOF);
    NSString *exceptionString;
    ${functionCall};

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, nullString(), String(exceptionString));
        return ${defaultEarlyReturnValue};
    }

    return ${defaultReturnValue};
}
EOF
            } elsif ($needsExceptionString && !$isVoidReturn) {
                push(@contents, <<EOF);
    String exceptionString;
    JSValueRef result = ${returnExpression};

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, nullString(), String(exceptionString));
        return ${defaultEarlyReturnValue};
    }

    return result;
}
EOF
            } elsif ($needsExceptionString && $isVoidReturn) {
                push(@contents, <<EOF);
    String exceptionString;
    ${functionCall};

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, nullString(), exceptionString);
        return ${defaultEarlyReturnValue};
    }

    return ${defaultReturnValue};
}
EOF
            } elsif ($isVoidReturn) {
                push(@contents, "    ${functionCall};\n\n");
                push(@contents, "    return ${defaultReturnValue};\n}\n");
            } else {
                # The safer cpp static analysis wants us to protect the value passed to toJS() but it is not
                # necessary for safety.
                push(@contents, "    SUPPRESS_UNCOUNTED_ARG return " . $returnExpression . ";\n}\n");
            }

            if ($isPropertyFunction) {
                push(@contents, <<EOF);

${functionSignature}
{
    RefPtr impl = to${implementationClassName}(context, thisObject);
    if (${functionEarlyReturnCondition}) [[unlikely]]
        return JSValueMakeUndefined(context);

EOF

                my $keyArgumentName = $specifiedParameters[0]->name;
                push(@contents, "    auto ${keyArgumentName} = toJSString(argumentCount > 0 ? " . $self->_platformTypeConstructor($interface, $specifiedParameters[0], "arguments[0]") . "_s : emptyString());\n");

                if ($isGetPropertyFunction) {
                    push(@contents, "\n");
                    push(@contents, "    return getProperty(context, thisObject, ${keyArgumentName}.get(), exception);\n");
                } elsif ($isSetPropertyFunction) {
                    my $valueArgumentName = $specifiedParameters[1]->name;
                    push(@contents, "    JSValueRef ${valueArgumentName} = argumentCount > 1 ? arguments[1] : JSValueMakeUndefined(context);\n\n");
                    push(@contents, "    setProperty(context, thisObject, ${keyArgumentName}.get(), ${valueArgumentName}, exception);\n\n");
                } elsif ($isDeletePropertyFunction) {
                    push(@contents, "\n");
                    push(@contents, "    deleteProperty(context, thisObject, ${keyArgumentName}.get(), exception);\n\n");
                } else {
                    die "Unknown property function";
                }

                push(@contents, "    return JSValueMakeUndefined(context);\n") if $isSetPropertyFunction || $isDeletePropertyFunction;

                push(@contents, "}\n");
            }

            push(@contents, <<EOF) if $conditionalString;
#endif // ${conditionalString}
EOF
        }
    }

    if (my @constants = @{$interface->constants}) {
        push(@contents, "\n// Constants\n");
        foreach my $constant (@constants) {
            my $getterName = $constant->name;
            my $constantValue = $constant->value;
            push(@contents, <<EOF);

JSValueRef ${className}::${getterName}(JSContextRef context, JSObjectRef object, JSStringRef, JSValueRef* exception)
{
    return JSValueMakeNumber(context, $constantValue);
}
EOF
        }
    }

    if (my @attributes = @{$interface->attributes}) {
        push(@contents, "\n// Attributes\n");
        foreach my $attribute (@attributes) {
            next if $attribute->extendedAttributes->{"NoImplementation"} or $attribute->extendedAttributes->{"Custom"};

            $self->_includeHeaders(\%contentsIncludes, $attribute->type, $attribute);

            my $getterName = $self->_getterName($attribute);
            my $call = _callString($idlType, $attribute, 0);

            my $needsFrame = $attribute->extendedAttributes->{"NeedsFrame"};
            my $needsFrameIdentifier = $attribute->extendedAttributes->{"NeedsFrameIdentifier"};
            my $needsPage = $attribute->extendedAttributes->{"NeedsPage"};
            my $needsPageIdentifier = $attribute->extendedAttributes->{"NeedsPageIdentifier"};

            my @methodSignatureNames = ();
            my @parameters = ();

            push(@methodSignatureNames, "context") if $attribute->extendedAttributes->{"NeedsScriptContext"};
            push(@parameters, "context") if $attribute->extendedAttributes->{"NeedsScriptContext"};

            push(@methodSignatureNames, "page") if $needsPage;
            push(@parameters, "*page") if $needsPage;

            push(@methodSignatureNames, "webPageProxyIdentifier") if $needsPageIdentifier;
            push(@parameters, "page->webPageProxyIdentifier()") if $needsPageIdentifier;

            push(@methodSignatureNames, "frame") if $needsFrame;
            push(@parameters, "*frame") if $needsFrame;

            push(@methodSignatureNames, "frameIdentifier") if $needsFrameIdentifier;
            push(@parameters, "frame->frameID()") if $needsFrameIdentifier;

            my $getterExpression = $self->_functionCall($attribute, \@methodSignatureNames, \@parameters, $interface, $getterName);

            my $getterEarlyReturnCondition = $earlyReturnCondition;

            push(@contents, <<EOF);

EOF

            my $platformString = platformString($attribute);
            push(@contents, <<EOF) if $platformString;
#if ${platformString}
EOF

            my $conditionalString = conditionalString($attribute);
            push(@contents, <<EOF) if $conditionalString;
#if ${conditionalString}
EOF

            push(@contents, <<EOF);
JSValueRef ${className}::${getterName}(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
EOF

            push(@contents, "    UNUSED_PARAM(propertyName);\n\n");

            push(@contents, <<EOF);
    RefPtr impl = to${implementationClassName}(context, object);
    if (${getterEarlyReturnCondition}) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called getter ${call} in %" PUBLIC_LOG_STRING " world", makeUTF8CString(toDebugString(impl->contentWorldType())));
EOF

            if ($needsPage || $needsPageIdentifier) {
                push(@contents, "\n");
                push(@contents, "    RefPtr page = toWebPage(context);\n");
                push(@contents, "    if (!page) [[unlikely]] {\n");
                push(@contents, "        RELEASE_LOG_ERROR(Extensions, \"Page could not be found for JSContextRef\");\n");
                push(@contents, "        return JSValueMakeUndefined(context);\n");
                push(@contents, "    }\n");
            }

            if ($needsFrame || $needsFrameIdentifier) {
                push(@contents, "\n");
                push(@contents, "    RefPtr frame = toWebFrame(context);\n");
                push(@contents, "    if (!frame) [[unlikely]] {\n");
                push(@contents, "        RELEASE_LOG_ERROR(Extensions, \"Frame could not be found for JSContextRef\");\n");
                push(@contents, "        return JSValueMakeUndefined(context);\n");
                push(@contents, "    }\n");
            }

            my $returnExpression = $attribute->extendedAttributes->{"EnumObject"} ? $self->_enumerationObjectExpression($attribute->extendedAttributes->{"EnumObject"}) : $self->_returnExpression($attribute, $getterExpression, $interface);

            push(@contents, <<EOF);

    SUPPRESS_UNCOUNTED_ARG return ${returnExpression};
}
EOF

            unless ($attribute->isReadOnly) {
                my $setterEarlyReturnCondition = $earlyReturnCondition;

                push(@contents, <<EOF);

bool ${className}::@{[$self->_setterName($attribute)]}(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
EOF

                push(@contents, "    UNUSED_PARAM(propertyName);\n\n");

                push(@contents, <<EOF);
    RefPtr impl = to${implementationClassName}(context, object);
    if (${setterEarlyReturnCondition}) [[unlikely]]
        return false;

    RELEASE_LOG_DEBUG(Extensions, "Called setter ${call} in %" PUBLIC_LOG_STRING " world", makeUTF8CString(toDebugString(impl->contentWorldType())));
EOF

                my $platformValue;
                if ($self->_hasAutomaticExceptions($attribute)) {
                    $platformValue = "platformValue";
                    push(@contents, "    " . $self->_platformTypeVariableDeclaration($interface, $attribute, "platformValue", "value") . "\n");
                    $self->_installAutomaticExceptions(\@contents, $interface, $attribute, $idlType, "platformValue", $attribute->name, "false", \%contentsIncludes, $attribute);
                } else {
                    $platformValue = $self->_platformTypeConstructor($interface, $attribute, "value");
                }

                if ($needsPage || $needsPageIdentifier) {
                    push(@contents, "\n");
                    push(@contents, "    RefPtr page = toWebPage(context);\n");
                    push(@contents, "    if (!page) [[unlikely]] {\n");
                    push(@contents, "        RELEASE_LOG_ERROR(Extensions, \"Page could not be found for JSContextRef\");\n");
                    push(@contents, "        return false;\n");
                    push(@contents, "    }\n");
                }

                if ($needsFrame || $needsFrameIdentifier) {
                    push(@contents, "\n");
                    push(@contents, "    RefPtr frame = toWebFrame(context);\n");
                    push(@contents, "    if (!frame) [[unlikely]] {\n");
                    push(@contents, "        RELEASE_LOG_ERROR(Extensions, \"Frame could not be found for JSContextRef\");\n");
                    push(@contents, "        return false;\n");
                    push(@contents, "    }\n");
                }

                push(@contents, <<EOF);
    impl->@{[$self->_setterName($attribute)]}(${platformValue});

    return true;
}
EOF
            }

            push(@contents, <<EOF) if $conditionalString;
#endif // ${conditionalString}
EOF
            push(@contents, <<EOF) if $platformString;
#endif // ${platformString}
EOF
        }
    }

    push(@contents, <<EOF);

} // namespace WebKit
EOF

    push(@contents, <<EOF) if $conditionalString;

#endif // ${conditionalString}
EOF

    unshift(@contents, map { "#include $_\n" } sort keys(%contentsIncludes));
    unshift(@contents, @contentsPrefix);

    return { name => $filename, contents => \@contents };
}

sub _getterName
{
    my ($self, $attribute) = @_;

    my $signature = $attribute;
    my $name = $signature->name;

    my %specialCases = (
        bubbles => "bubbles",
    );

    return $specialCases{$name} if defined $specialCases{$name};
    return "is" . $$self{codeGenerator}->WK_ucfirst($name) if $signature->type->name eq "boolean";
    return $signature->extendedAttributes->{"ImplementedAs"} if $signature->extendedAttributes->{"ImplementedAs"};
    return $name;
}

sub _hasAutomaticExceptions
{
    my ($self, $signature) = @_;

    return $signature->extendedAttributes->{"CannotBeEmpty"} || $signature->extendedAttributes->{"Serialization"} eq "JSON" || $signature->extendedAttributes->{"Serialization"} eq "Message" || $signature->extendedAttributes->{"NSArray"}
        || $signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"NSObject"} || $signature->extendedAttributes->{"URL"} || $signature->extendedAttributes->{"JSONValue"}
        || $signature->extendedAttributes->{"JSONObject"} || $signature->type->name eq "function" || $$self{codeGenerator}->IsPrimitiveType($signature->type) || $$self{codeGenerator}->IsStringType($signature->type);
}

sub _includeHeaders
{
    my ($self, $headers, $idlType, $signature) = @_;

    $$headers{'"WebFrame.h"'} = 1 if $signature->extendedAttributes->{"NeedsFrame"} || $signature->extendedAttributes->{"NeedsFrameIdentifier"};

    return unless defined $idlType;

    my $idlTypeName = $idlType->name;
    return if $idlTypeName eq "DOMString" or $idlTypeName eq "DOMWindow" or $idlTypeName eq "any" or $idlTypeName eq "void" or $idlTypeName eq "boolean" or $idlTypeName eq "function" or $idlTypeName eq "array" or $$self{codeGenerator}->IsPrimitiveType($idlType) or $self->_isParsedArgumentType($idlType, $signature);

    $$headers{'"' . _className($idlType) . '.h"'} = 1;
    $$headers{'"' . _implementationClassName($idlType) . '.h"'} = 1;
}

sub _implementationClassName
{
    my ($idlType) = @_;
    return $idlType->name;
}

sub _scriptClassName
{
    my ($classIDLType) = @_;
    my $className = CodeGenerator::WK_lcfirst(undef, _publicClassName($classIDLType));

    # Some cases make more sense to not include the parent name,
    # those are represented with empty strings, and it excludes
    # the call path in the output.
    my %specialCases = (
        devTools => "devtools",
        devToolsElementsPanel => "devtools.panels.elements",
        devToolsExtensionPanel => "",
        devToolsExtensionSidebarPane => "",
        event => "",
        localization => "i18n",
        namespace => "browser",
        webNavigationEvent => "",
        webPageNamespace => "browser",
        webRequestEvent => "",
        windowEvent => "",
    );

    $className = $specialCases{$className} if defined $specialCases{$className};

    # Transform all other devTools classes from devToolFoo to devtools.foo
    $className =~ s/^(devTools)(\w)/\L$1.\L$2/g;

    return $className;
}

sub _callString
{
    my ($classIDLType, $functionOrAttribute, $isFunction) = @_;

    my $className = _scriptClassName($classIDLType);
    my $name = $functionOrAttribute->name;

    my $call = "${className}.${name}";
    $call = $name unless $className;
    $call .= "()" if $isFunction;

    return $call;
}

sub _installArgumentTypeExceptions
{
    my ($self, $contents, $signature, $classIDLType, $variable, $variableLabel, $result, $contentsIncludes, $functionOrAttribute, $isFunction, $indentLevel) = @_;

    return 0 if $self->_isParsedArgumentType($signature->type, $signature);

    my $indentString = ' ' x (4 * $indentLevel);
    my $call = _callString($classIDLType, $functionOrAttribute, $isFunction);
    my $condition = $self->_javaScriptTypeCondition($signature, $variable);
    my $hasExceptions = 0;

    if ($$self{codeGenerator}->IsStringType($signature->type)) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);
${indentString}if (!($condition)) [[unlikely]] {
${indentString}    *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "a string is expected"_s);
${indentString}    return ${result};
${indentString}}

EOF
    }

    if ($$self{codeGenerator}->IsPrimitiveType($signature->type) && $signature->type->name ne "boolean") {
        $hasExceptions = 1;

        push(@$contents, <<EOF);
${indentString}if (!($condition)) [[unlikely]] {
${indentString}    *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "a number is expected"_s);
${indentString}    return ${result};
${indentString}}

EOF
    }

    if ($$self{codeGenerator}->IsPrimitiveType($signature->type) && $signature->type->name eq "boolean") {
        $hasExceptions = 1;

        push(@$contents, <<EOF);
${indentString}if (!($condition)) [[unlikely]] {
${indentString}    *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "a boolean is expected"_s);
${indentString}    return ${result};
${indentString}}

EOF
    }

    if ($signature->type->name eq "any" && ($signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"JSONValue"} || $signature->extendedAttributes->{"Serialization"})) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);
${indentString}if (!($condition)) [[unlikely]] {
${indentString}    *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "an object is expected"_s);
${indentString}    return ${result};
${indentString}}

EOF
    }

    if ($signature->type->name eq "any" && !($signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"JSONValue"} || $signature->extendedAttributes->{"Serialization"}) && !$signature->extendedAttributes->{"NSObject"} && !$signature->extendedAttributes->{"JSONObject"} && !$signature->extendedAttributes->{"ValuesAllowed"}) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);
${indentString}if (!($condition)) [[unlikely]] {
${indentString}    *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "an object is expected"_s);
${indentString}    return ${result};
${indentString}}

EOF
    }

    if ($signature->type->name eq "array") {
        $hasExceptions = 1;

        push(@$contents, <<EOF);
${indentString}if (!($condition)) [[unlikely]] {
${indentString}    *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "an array is expected"_s);
${indentString}    return ${result};
${indentString}}

EOF
    }

    if ($signature->type->name eq "function") {
        $hasExceptions = 1;

        push(@$contents, <<EOF);
${indentString}if (!($condition)) [[unlikely]] {
${indentString}    *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "a function is expected"_s);
${indentString}    return ${result};
${indentString}}

EOF
    }

    return $hasExceptions;
}

sub _installAutomaticExceptions
{
    my ($self, $contents, $interface, $signature, $classIDLType, $variable, $variableLabel, $result, $contentsIncludes, $functionOrAttribute, $isFunction) = @_;

    my $call = _callString($classIDLType, $functionOrAttribute, $isFunction);

    my $hasExceptions = 0;

    if ($$self{codeGenerator}->IsPrimitiveType($signature->type) && $signature->type->name ne "boolean") {
        my ($minimum, $maximum) = ($signature->extendedAttributes->{"Minimum"}, $signature->extendedAttributes->{"Maximum"});

        if (defined $minimum || defined $maximum) {
            $hasExceptions = 1;

            my @conditions = ();
            push(@conditions, "${variable} < ${minimum}") if defined $minimum;
            push(@conditions, "${variable} > ${maximum}") if defined $maximum;

            my $message = "makeString(\"it must be at least \"_s, ${minimum})" if defined $minimum && !defined $maximum;
            $message = "\"it must not be negative\"_s" if defined $minimum && $minimum eq "0" && !defined $maximum;
            $message = "makeString(\"it must be at most \"_s, ${maximum})" if !defined $minimum && defined $maximum;
            $message = "makeString(\"it must specify a value between \"_s, ${minimum}, \" and \"_s, ${maximum})" if defined $minimum && defined $maximum;

            push(@$contents, <<EOF);

    if (@{[join(" || ", @conditions)]}) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, ${message});
        return ${result};
    }
EOF
        }
    }

    if ($self->_isParsedArgumentType($signature->type, $signature)) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (!$variable) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, ${variable}.error());
        return ${result};
    }
EOF
    }

    # A string, or a sequence of strings, that has nothing in it, or an empty string in it.
    if ($signature->extendedAttributes->{"CannotBeEmpty"} && $self->_isParsedArgumentType($signature->type, $signature) && !_isOptionalArgument($signature)) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (webExtensionValueIsEmpty(*${variable})) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "it must not be empty"_s);
        return ${result};
    }

    if (webExtensionValueHasEmptyString(*${variable})) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "it must not contain an empty string"_s);
        return ${result};
    }
EOF
    }

    if ($signature->type->name eq "any" && $signature->extendedAttributes->{"Serialization"} && $signature->extendedAttributes->{"Serialization"} eq "JSON") {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (*exception) [[unlikely]]
        return ${result};

    if (!$variable) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "a JSON serializable value is expected"_s);
        return ${result};
    }
EOF
    }

    if ($signature->type->name eq "any" && $signature->extendedAttributes->{"Serialization"} && $signature->extendedAttributes->{"Serialization"} eq "Message") {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (*exception) [[unlikely]]
        return ${result};

    if (!$variable) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, impl->usesStructuredCloneMessageSerialization() ? "a serializable value is expected"_s : "a JSON serializable value is expected"_s);
        return ${result};
    }
EOF
    }

    if ($$self{codeGenerator}->IsStringType($signature->type) && !_isOptionalArgument($signature) && ($signature->extendedAttributes->{"URL"} && !$interface->extendedAttributes->{"UseCPPAPI"})) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (!$variable) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "a string is expected"_s);
        return ${result};
    }
EOF
    }

    # A parsed argument, such as one with a custom type, has already been checked by its parser.
    if ($$self{codeGenerator}->IsPrimitiveType($signature->type) && $signature->type->name ne "boolean" && !_isOptionalArgument($signature) && !$self->_isParsedArgumentType($signature->type, $signature)) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (!std::isfinite($variable)) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "a number is expected"_s);
        return ${result};
    }
EOF
    }

    if ($signature->type->name eq "any" && ($signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"JSONValue"} || $signature->extendedAttributes->{"JSONObject"} || $signature->extendedAttributes->{"NSObject"}) && !_isOptionalArgument($signature)) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (!$variable) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "an object is expected"_s);
        return ${result};
    }
EOF
    }

    if (!$interface->extendedAttributes->{"UseCPPAPI"} && $signature->type->name eq "any" && !($signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"JSONValue"} || $signature->extendedAttributes->{"JSONObject"} || $signature->extendedAttributes->{"NSObject"} || $signature->extendedAttributes->{"Serialization"}) && !_isOptionalArgument($signature) && !$signature->extendedAttributes->{"ValuesAllowed"}) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if ($variable && !$variable.isObject) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "an object is expected"_s);
        return ${result};
    }
EOF
    }

    if ($interface->extendedAttributes->{"UseCPPAPI"} && $signature->type->name eq "any" && !($signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"JSONValue"} || $signature->extendedAttributes->{"JSONObject"} || $signature->extendedAttributes->{"NSObject"} || $signature->extendedAttributes->{"Serialization"}) && !_isOptionalArgument($signature) && !$signature->extendedAttributes->{"ValuesAllowed"}) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if ($variable && !JSValueIsObject(context, $variable)) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "an object is expected"_s);
        return ${result};
    }
EOF
    }

    if (!$signature->extendedAttributes->{"Vector"} && $signature->type->name eq "array" && !_isOptionalArgument($signature)) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (!$variable) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "an array is expected"_s);
        return ${result};
    }
EOF
    }

    if ($signature->extendedAttributes->{"CannotBeEmpty"} && !$self->_isParsedArgumentType($signature->type, $signature)) {
        $hasExceptions = 1;

        my $isEmptyCheck;
        if ($signature->extendedAttributes->{"URL"}) {
            $isEmptyCheck = "!${variable}.relativeString.length";
        } elsif ($signature->type->name eq "DOMString") {
            $isEmptyCheck = "${variable}.isEmpty()";
        } else {
            $isEmptyCheck = "!${variable}.length";
        }

        push(@$contents, <<EOF);

    if (${isEmptyCheck}) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "it cannot be empty"_s);
        return ${result};
    }
EOF
    }

    if ($signature->type->name eq "function" && !$signature->extendedAttributes->{"CallbackHandler"} && !_isOptionalArgument($signature)) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if ($variable && !JSObjectIsFunction(context, $variable)) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "a function is expected"_s);
        return ${result};
    }
EOF
    }

    if ($signature->type->name eq "function" && $signature->extendedAttributes->{"CallbackHandler"} && !_isOptionalArgument($signature)) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (!$variable) [[unlikely]] {
        *exception = toJSError(context, "${call}"_s, "${variableLabel}"_s, "a function is expected"_s);
        return ${result};
    }
EOF
    }

    return $hasExceptions;
}

sub _yearString
{
    my (undef, undef, undef, undef, undef, $year) = localtime(time);

    return $year;
}

sub _nullStringPolicy
{
    my ($signature) = @_;

    die "Both NullAsNullString and ConvertNullAndUndefinedToString were specified" if $signature->extendedAttributes->{"ConvertNullToNullString"} && $signature->extendedAttributes->{"ConvertNullAndUndefinedToString"};

    return "NullStringPolicy::NullAsNullString" if $signature->extendedAttributes->{"ConvertNullToNullString"};
    return "NullStringPolicy::NoNullString" if $signature->extendedAttributes->{"ConvertNullAndUndefinedToString"};
    return "NullStringPolicy::NullAndUndefinedAsNullString";
}

sub _publicClassName
{
    my ($idlType) = @_;
    my $idlTypeName = $idlType->name;
    $idlTypeName =~ s/^WebExtensionAPI//;
    return $idlTypeName;
}

sub _parentClassName
{
    my ($self, $interface) = @_;

    my $parentInterface = _parentInterface($interface);
    return $parentInterface ? _className($parentInterface) : "JSWebExtensionWrapper";
}

sub _parentClassRefGetterExpression
{
    my ($self, $interface) = @_;

    my $parentInterface = _parentInterface($interface);
    return $parentInterface ? $self->_classRefGetter($parentInterface) . "()" : "nullptr";
}

sub _parentInterface
{
    my ($interface) = @_;
    return $interface->parentType;
}

sub _javaScriptTypeCondition
{
    my ($self, $signature, $argument) = @_;

    my $idlType = $signature->type;

    return undef unless defined $idlType;

    my $idlTypeName = $idlType->name;
    my $nullOrUndefined = "";
    $nullOrUndefined = " || JSValueIsNull(context, ${argument}) || JSValueIsUndefined(context, ${argument})" if _isOptionalArgument($signature);

    return "WebExtensionValueParser<" . $self->_parsedCppType($idlType, "argument", $signature->extendedAttributes->{"CustomType"}) . ">::matches(context, ${argument})${nullOrUndefined}" if $self->_isParsedArgumentType($idlType, $signature);

    return "isDictionary(context, ${argument}) || JSValueIsString(context, ${argument})${nullOrUndefined}" if $idlTypeName eq "any" && ($signature->extendedAttributes->{"NSObject"} || $signature->extendedAttributes->{"JSONObject"}) && $signature->extendedAttributes->{"DOMString"};
    return "(!JSValueIsNull(context, ${argument}) && !JSValueIsUndefined(context, ${argument}) && !JSObjectIsFunction(context, JSValueToObject(context, ${argument}, nullptr)))${nullOrUndefined}" if $idlTypeName eq "any" && $signature->extendedAttributes->{"Serialization"};
    return "isDictionary(context, ${argument})${nullOrUndefined}" if $idlTypeName eq "any" && ($signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"JSONValue"});
    return "JSValueIsObject(context, ${argument})${nullOrUndefined}" if $idlTypeName eq "any" && ($signature->extendedAttributes->{"NSObject"} || $signature->extendedAttributes->{"JSONObject"});
    return "JSValueIsObject(context, ${argument})${nullOrUndefined}" if $idlTypeName eq "any" && !$signature->extendedAttributes->{"ValuesAllowed"};
    return "(JSValueIsObject(context, ${argument}) && JSObjectIsFunction(context, JSValueToObject(context, ${argument}, nullptr)))${nullOrUndefined}" if $idlTypeName eq "function";
    return "JSValueIsBoolean(context, ${argument})${nullOrUndefined}" if $idlTypeName eq "boolean";
    return "JSValueIsArray(context, ${argument})${nullOrUndefined}" if $idlTypeName eq "array";
    return "JSValueIsString(context, ${argument})${nullOrUndefined}" if $$self{codeGenerator}->IsStringType($idlType);
    return "JSValueIsNumber(context, ${argument})${nullOrUndefined}"  if $$self{codeGenerator}->IsPrimitiveType($idlType);
    return $argument;
}

sub _platformType
{
    my ($self, $idlType, $interface, $signature) = @_;

    return undef unless defined $idlType;

    my $idlTypeName = $idlType;
    $idlTypeName = $idlType->name if ref($idlType) eq "IDLType";

    my $arrayType = $signature->extendedAttributes->{"Vector"};

    return "RefPtr<WebExtensionCallbackHandler>" if $idlTypeName eq "function" && $signature->extendedAttributes->{"CallbackHandler"};
    return "NSArray" if !$interface->extendedAttributes->{"UseCPPAPI"} && $idlTypeName eq "array";
    return "Vector<$arrayType>" if $interface->extendedAttributes->{"UseCPPAPI"} && $idlTypeName eq "array" && $arrayType ne "JSValueRef";
    return "Vector<Protected<$arrayType>>" if $interface->extendedAttributes->{"UseCPPAPI"} && $idlTypeName eq "array" && $arrayType eq "JSValueRef";
    return "std::optional<WebExtensionMessage>" if $idlTypeName eq "any" && $signature->extendedAttributes->{"Serialization"} && $signature->extendedAttributes->{"Serialization"} eq "Message";
    return "String" if $idlTypeName eq "any" && $signature->extendedAttributes->{"Serialization"};
    return "RefPtr<JSON::Value>" if $idlTypeName eq "any" && $signature && ($signature->extendedAttributes->{"JSONValue"} || $signature->extendedAttributes->{"JSONObject"});
    return "NSDictionary" if $idlTypeName eq "any" && $signature && $signature->extendedAttributes->{"NSDictionary"};
    return "NSObject" if $idlTypeName eq "any" && $signature && $signature->extendedAttributes->{"NSObject"};
    return "JSValue" if !$interface->extendedAttributes->{"UseCPPAPI"} && ($idlTypeName eq "DOMWindow" || $idlTypeName eq "function" || $idlTypeName eq "any");
    return "JSValueRef" if $interface->extendedAttributes->{"UseCPPAPI"} && ($idlTypeName eq "DOMWindow" || $idlTypeName eq "function" || $idlTypeName eq "any");
    return "bool" if $idlTypeName eq "boolean";

    return unless ref($idlType) eq "IDLType";

    return "NSURL" if !$interface->extendedAttributes->{"UseCPPAPI"} && $$self{codeGenerator}->IsStringType($idlType) && $signature && $signature->extendedAttributes->{"URL"};
    return "URL" if $interface->extendedAttributes->{"UseCPPAPI"} && $$self{codeGenerator}->IsStringType($idlType) && $signature && $signature->extendedAttributes->{"URL"};
    return "String" if $$self{codeGenerator}->IsStringType($idlType);
    return "double" if $$self{codeGenerator}->IsPrimitiveType($idlType);

    return _implementationClassName($idlType);
}

sub _platformTypeConstructor
{
    my ($self, $interface, $signature, $argumentName) = @_;

    my $idlType = $signature->type;

    my $idlTypeName = $idlType;
    $idlTypeName = $idlType->name if ref($idlType) eq "IDLType";

    my $nullStringPolicy = _nullStringPolicy($signature);
    my $arrayType = $interface->extendedAttributes->{"UseCPPAPI"} ? $signature->extendedAttributes->{"Vector"} : $signature->extendedAttributes->{"NSArray"};

    # A custom type is parsed like any other, even when the IDL type is any.
    if ($idlTypeName eq "any" && !$signature->extendedAttributes->{"CustomType"}) {
        return "serializeJSObject(context, $argumentName, exception)" if $signature->extendedAttributes->{"Serialization"} && $signature->extendedAttributes->{"Serialization"} eq "JSON";
        return "serializeMessage(context, $argumentName, impl->usesStructuredCloneMessageSerialization(), exception)" if $signature->extendedAttributes->{"Serialization"} && $signature->extendedAttributes->{"Serialization"} eq "Message";
        return "toNSDictionary(context, $argumentName, NullValuePolicy::Allowed, ValuePolicy::StopAtTopLevel)" if $signature->extendedAttributes->{"NSDictionary"} && $signature->extendedAttributes->{"NSDictionary"} eq "StopAtTopLevel";
        return "toNSDictionary(context, $argumentName, NullValuePolicy::Allowed)" if $signature->extendedAttributes->{"NSDictionary"} && $signature->extendedAttributes->{"NSDictionary"} eq "NullAllowed";
        return "toNSDictionary(context, $argumentName, NullValuePolicy::NotAllowed)" if $signature->extendedAttributes->{"NSDictionary"};
        return "toJSONValue(context, $argumentName, NullValuePolicy::Allowed, ValuePolicy::StopAtTopLevel)" if $signature->extendedAttributes->{"JSONValue"} && $signature->extendedAttributes->{"JSONValue"} eq "StopAtTopLevel";
        return "toJSONValue(context, $argumentName, NullValuePolicy::Allowed)" if $signature->extendedAttributes->{"JSONValue"} && $signature->extendedAttributes->{"JSONValue"} eq "NullAllowed";
        return "toJSONValue(context, $argumentName, NullValuePolicy::NotAllowed)" if $signature->extendedAttributes->{"JSONValue"};
        return "fromJSValue(context, $argumentName, NullValuePolicy::Allowed, ValuePolicy::StopAtTopLevel)" if $signature->extendedAttributes->{"JSONObject"} && $signature->extendedAttributes->{"JSONObject"} eq "StopAtTopLevel";
        return "fromJSValue(context, $argumentName, NullValuePolicy::Allowed)" if $signature->extendedAttributes->{"JSONObject"} && $signature->extendedAttributes->{"JSONObject"} eq "NullAllowed";
        return "fromJSValue(context, $argumentName, NullValuePolicy::NotAllowed)" if $signature->extendedAttributes->{"JSONObject"};
        return "toNSObject(context, $argumentName, Nil, NullValuePolicy::Allowed, ValuePolicy::StopAtTopLevel)" if $signature->extendedAttributes->{"NSObject"} && $signature->extendedAttributes->{"NSObject"} eq "StopAtTopLevel";
        return "toNSObject(context, $argumentName, Nil, NullValuePolicy::Allowed)" if $signature->extendedAttributes->{"NSObject"} && $signature->extendedAttributes->{"NSObject"} eq "NullAllowed";
        return "toNSObject(context, $argumentName)" if $signature->extendedAttributes->{"NSObject"};
        return "$argumentName" if $interface->extendedAttributes->{"UseCPPAPI"};
        return "toJSValue(context, $argumentName)";
    }

    return "toJSCallbackHandler(context, $argumentName, protect(impl->runtime()))" if $idlTypeName eq "function" && $signature->extendedAttributes->{"CallbackHandler"};
    return "toNSArray(context, $argumentName, $arrayType.class)" if !$interface->extendedAttributes->{"UseCPPAPI"} && $idlTypeName eq "array" && $arrayType;
    return "toNSArray(context, $argumentName)" if !$interface->extendedAttributes->{"UseCPPAPI"} && $idlTypeName eq "array";
    return "toVector<$arrayType>(context, $argumentName)" if $interface->extendedAttributes->{"UseCPPAPI"} && $idlTypeName eq "array" && $arrayType ne "JSValueRef";
    return "toVector<Protected<$arrayType>>(context, $argumentName)" if $interface->extendedAttributes->{"UseCPPAPI"} && $idlTypeName eq "array" && $arrayType eq "JSValueRef";
    return "JSValueToBoolean(context, $argumentName)" if $idlTypeName eq "boolean";
    return "$argumentName" if $interface->extendedAttributes->{"UseCPPAPI"} && $idlTypeName eq "function";
    return "toJSValue(context, $argumentName)" if $idlTypeName eq "function";

    return unless ref($idlType) eq "IDLType";

    if ($self->_isParsedArgumentType($idlType, $signature)) {
        my $cppType = $self->_parsedCppType($idlType, "argument", $signature->extendedAttributes->{"CustomType"});

        return "parseOptionalWebExtensionValue<${cppType}>(context, $argumentName)" if _isOptionalArgument($signature);
        return "WebExtensionValueParser<${cppType}>::parse(context, $argumentName, { })";
    }

    return "[NSURL URLWithString:toString(context, $argumentName, $nullStringPolicy).createNSString().get()]" if !$interface->extendedAttributes->{"UseCPPAPI"} && $$self{codeGenerator}->IsStringType($idlType) && $signature->extendedAttributes->{"URL"};
    return "URL { toString(context, $argumentName, $nullStringPolicy) }" if $interface->extendedAttributes->{"UseCPPAPI"} && $$self{codeGenerator}->IsStringType($idlType) && $signature->extendedAttributes->{"URL"};
    return "toString(context, $argumentName, $nullStringPolicy)" if $$self{codeGenerator}->IsStringType($idlType);
    return "JSValueToNumber(context, $argumentName, nullptr)" if $$self{codeGenerator}->IsPrimitiveType($idlType);
    return "to" . _implementationClassName($idlType) . "(context, $argumentName)";
}

sub _platformTypeVariableDeclaration
{
    my ($self, $interface, $signature, $variableName, $argumentName, $condition, $hideType) = @_;

    my $idlType = $signature->type;
    my $idlTypeName = $idlType->name;
    my $platformType = $self->_platformType($idlType, $interface, $signature);
    my $constructor = $self->_platformTypeConstructor($interface, $signature, $argumentName) if $argumentName;

    if ($self->_isParsedArgumentType($idlType, $signature)) {
        my $expectedType = $self->_parsedArgumentResultType($idlType, _isOptionalArgument($signature), "argument", $signature->extendedAttributes->{"CustomType"});

        return "$variableName = $constructor;" if $constructor && $hideType;
        return "$expectedType $variableName = $constructor;" if $constructor;
        return "$expectedType $variableName;" if _isOptionalArgument($signature);

        # A required argument is always assigned later, and its type may have no default constructor.
        return "$expectedType $variableName = makeUnexpected(WebExtensionValueError { \"a required argument is missing\"_s });";
    }

    my %objCTypes = (
        "JSValue"       => 1,
        "NSArray"       => 1,
        "NSDictionary"  => 1,
        "NSURL"         => 1,
        "NSObject"      => 1,
    );

    my $isObjCType = $objCTypes{$platformType};

    my $nullValue = "nullptr";
    $nullValue = "false" if $platformType eq "bool";
    $nullValue = "std::numeric_limits<double>::quiet_NaN()" if $platformType eq "double";
    $nullValue = "nil" if $isObjCType;
    $nullValue = "nullString()" if $platformType eq "String";
    $nullValue = "std::nullopt" if $platformType eq "std::optional<WebExtensionMessage>";
    $nullValue = "JSValueMakeUndefined(context)" if $platformType eq "JSValueRef";
    $nullValue = "{  }" if $signature->extendedAttributes->{"URL"} && $interface->extendedAttributes->{"UseCPPAPI"};

    my $defaultValue = $signature->extendedAttributes->{"DefaultValue"};
    if (defined $defaultValue && $platformType eq "double") {
        $nullValue = $defaultValue eq "Max" ? "std::numeric_limits<double>::max()" : $defaultValue;
    } elsif (defined $defaultValue) {
        die "DefaultValue extended attribute is currently only supported for numeric types";
    }

    if ($platformType eq "JSValueRef" or $platformType eq "JSObjectRef" or $platformType eq "RefPtr<WebExtensionCallbackHandler>" or $platformType eq "RefPtr<JSON::Value>" or $platformType eq "double" or $platformType eq "bool" or $platformType eq "String" or $platformType eq "std::optional<WebExtensionMessage>" or $signature->extendedAttributes->{"Vector"} or ($interface->extendedAttributes->{"UseCPPAPI"} && $signature->extendedAttributes->{"URL"})) {
        $platformType .= " ";
    } else {
        $platformType .= $isObjCType ? " *" : "* ";
    }

    $platformType = "" if $hideType;

    return "$platformType$variableName = $condition && $constructor;" if $condition && $platformType eq "bool ";
    return "$platformType$variableName = $condition ? $constructor : $nullValue;" if $condition;
    return "$platformType$variableName = $constructor;" if $constructor;
    return "$platformType$variableName;" if !$hideType && ($platformType =~ /^RefPtr/ || $isObjCType);
    return "$platformType$variableName = $nullValue;";
}

sub _functionCall
{
    my ($self, $signature, $methodSignatureNamesRef, $parametersRef, $interface, $getterName) = @_;

    my @methodSignatureNames = @$methodSignatureNamesRef;
    my @parameters = @$parametersRef;

    my $functionName = "impl->" . $signature->name;
    $functionName = "impl->" . $getterName if $getterName;
    $functionName = "impl->" . $signature->extendedAttributes->{"ImplementedAs"} if $signature->extendedAttributes->{"ImplementedAs"};
    return $functionName . "(" . join(", ", @parameters) . ")";
}

sub _returnExpression
{
    my ($self, $signature, $expression, $interface) = @_;

    my $convertNullStringAttribute = $signature->extendedAttributes->{"ConvertNullStringTo"};
    my $nullOrEmptyString = "NullOrEmptyString::NullStringAsEmptyString";
    $nullOrEmptyString = "NullOrEmptyString::NullStringAsNull" if defined $convertNullStringAttribute && $convertNullStringAttribute eq "Null";

    my $returnIDLType = $signature->type;
    my $returnIDLTypeName = $returnIDLType->name;

    return "deserializeJSONString(context, $expression)" if $returnIDLTypeName eq "any" && $signature->extendedAttributes->{"Serialization"} && $signature->extendedAttributes->{"Serialization"} eq "JSON";
    return "($expression).toJSValue(context)" if $returnIDLTypeName eq "any" && $signature->extendedAttributes->{"Serialization"} && $signature->extendedAttributes->{"Serialization"} eq "Message";
    return "toJSValueRefOrJSNull(context, $expression)" if $returnIDLTypeName eq "any" || $returnIDLTypeName eq "DOMWindow";
    return "toJSValueRef(context, ${expression}, $nullOrEmptyString)" if $$self{codeGenerator}->IsStringType($returnIDLType);
    return "JSValueMakeUndefined(context)" if $returnIDLTypeName eq "void";
    return "JSValueMakeBoolean(context, ${expression})" if $returnIDLTypeName eq "boolean";
    return "JSValueMakeNumber(context, ${expression})" if $$self{codeGenerator}->IsPrimitiveType($returnIDLType);
    return "toJS(context, getPtr(${expression}))";
}

sub _setterName
{
    my ($self, $attribute) = @_;

    my $name = $attribute->name;

    my %specialCases = (
        url => "URL"
    );

    return "set" . $specialCases{$name} if defined $specialCases{$name};
    return "set" . $$self{codeGenerator}->WK_ucfirst($name);
}

sub _staticFunctionsGetterImplementation
{
    my ($self, $interface) = @_;

    my $mapFunction = sub {
        return if $_->extendedAttributes->{"Dynamic"} or $_->extendedAttributes->{"MainWorldOnly"};

        my $name = $_->name;
        my @attributes = ();
        push(@attributes, "kJSPropertyAttributeDontEnum") if $_->extendedAttributes->{"DontEnum"};
        my $jsproperties = scalar @attributes == 0 ? "kJSPropertyAttributeNone" : join(" | ", @attributes);
        my $conditionalString = conditionalString($_);

        return "#if ${conditionalString}\n        { \"$name\", $name, $jsproperties },\n        #endif" if $conditionalString;
        return "{ \"$name\", $name, $jsproperties },";
    };

    my @initializers = map(&$mapFunction, @{$interface->operations});
    return $self->_staticFunctionsOrValuesOrConstantsGetterImplementation($interface, "function", "{ nullptr, nullptr, kJSPropertyAttributeNone },", @initializers);
}

sub _staticFunctionsOrValuesOrConstantsGetterImplementation
{
    my ($self, $interface, $functionOrValueOrConstant, $arrayTerminator, @initializers) = @_;

    my $className = _className($interface->type);
    my $uppercaseFunctionOrValueOrConstant = $$self{codeGenerator}->WK_ucfirst($functionOrValueOrConstant);
    my $functionOrValueType = $uppercaseFunctionOrValueOrConstant;
    $functionOrValueType = "Value" if $functionOrValueType eq "Constant";

    my $result = <<EOF;
const JSStatic${functionOrValueType}* ${className}::static${uppercaseFunctionOrValueOrConstant}s()
{
EOF

    return $result . "    return nullptr;\n}\n" unless @initializers;

    $result .= <<EOF
    static const JSStatic${functionOrValueType} ${functionOrValueOrConstant}s[] = {
        @{[join("\n        ", @initializers)]}
        ${arrayTerminator}
    };

    return ${functionOrValueOrConstant}s;
}
EOF
}

sub _staticValuesGetterImplementation
{
    my ($self, $interface) = @_;

    my $mapFunction = sub {
        return if $_->extendedAttributes->{"NoImplementation"} or $_->extendedAttributes->{"Dynamic"} or $_->extendedAttributes->{"MainWorldOnly"};

        my $attributeName = $_->name;
        my $attributeIsReadonly = $_->isReadOnly;
        my $getterName = $self->_getterName($_);
        my $setterName = $attributeIsReadonly ? "nullptr" : $self->_setterName($_);
        my @attributes = ();
        push(@attributes, "kJSPropertyAttributeDontEnum") if $_->extendedAttributes->{"DontEnum"};
        my $jsproperties = scalar @attributes == 0 ? "kJSPropertyAttributeNone" : join(" | ", @attributes);
        my $conditionalString = conditionalString($_);
        my $platformString = platformString($_);
        my $platformConditional = $platformString ? "\n#if ${platformString}\n" : "";
        my $platformConditionalEnd = $platformString ? "#endif // ${platformString}" : "";

        return "${platformConditional}#if ${conditionalString}\n        { \"$attributeName\", $getterName, $setterName, $jsproperties },\n        #endif${platformConditionalEnd}" if $conditionalString;
        return "${platformConditional}        { \"$attributeName\", $getterName, $setterName, $jsproperties },\n        ${platformConditionalEnd}" if $platformString;
        return "{ \"$attributeName\", $getterName, $setterName, $jsproperties },";
    };

    my @initializersWithConstants = (map(&$mapFunction, @{$interface->attributes}), $self->_staticConstants($interface));

    return $self->_staticFunctionsOrValuesOrConstantsGetterImplementation($interface, "value", "{ nullptr, nullptr, nullptr, kJSPropertyAttributeNone },", @initializersWithConstants);
}

sub _staticConstants
{
    my ($self, $interface) = @_;
    my @initializers = ();
    foreach my $constant (@{$interface->constants}) {
        my $constantName = $constant->name;
        push(@initializers, "{ \"$constantName\", $constantName, nullptr, kJSPropertyAttributeNone }");
    }
    return @initializers;
}

sub _staticConstantsGetterImplementation
{
    my ($self, $interface) = @_;
    return $self->_staticFunctionsOrValuesOrConstantsGetterImplementation($interface, "constant", "{ nullptr, nullptr, nullptr, kJSPropertyAttributeNone },", $self->_staticConstants($interface));
}

sub _hasInstanceImplementation
{
    my ($self, $interface) = @_;

    my $className = _className($interface->type);
    my $classRefGetter = $self->_classRefGetter($interface->type);

    my $result = <<EOF;
bool ${className}::hasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception)
{
    return JSValueIsObjectOfClass(ctx, possibleInstance, ${classRefGetter}());
}
EOF
}

sub _dynamicAttributesImplementation
{
    my ($self, $interface, $hasDynamicProperties, $hasMainWorldOnlyProperties) = @_;

    return unless $hasDynamicProperties or $hasMainWorldOnlyProperties;

    my $idlType = $interface->type;
    my $idlTypeName = $idlType->name;
    my $className = _className($idlType);
    my $implementationClassName = _implementationClassName($idlType);

    my @contents = ();

    push(@contents, <<EOF);

void ${className}::getPropertyNames(JSContextRef context, JSObjectRef thisObject, JSPropertyNameAccumulatorRef propertyNames)
{
    RefPtr impl = to${implementationClassName}(context, thisObject);
    if (!impl) [[unlikely]]
        return;

    RefPtr page = toWebPage(context);
EOF
    my $generateCondition = sub {
        my ($interface, $middleCondition) = @_;
        my $name = $interface->name;
        my @conditions = ();
        push(@conditions, "isForMainWorld") if $interface->extendedAttributes->{"MainWorldOnly"};
        push(@conditions, $middleCondition) if $middleCondition;
        push(@conditions, "impl->isPropertyAllowed(\"${name}\"_s, page.get())") if $interface->extendedAttributes->{"Dynamic"};
        return join(" && ", @conditions);
    };

    push(@contents, "    bool isForMainWorld = impl->isForMainWorld();\n") if $hasMainWorldOnlyProperties;
    push(@contents, "\n") if $hasMainWorldOnlyProperties and $hasDynamicProperties;

    if ($hasMainWorldOnlyProperties and !$hasDynamicProperties) {
        push(@contents, "    if (!isForMainWorld)\n");
        push(@contents, "        return;\n\n");
    }

    my $mapFunction = sub {
        return unless $_->extendedAttributes->{"Dynamic"} or $_->extendedAttributes->{"MainWorldOnly"};

        my $name = $_->name;
        return "    SUPPRESS_UNCOUNTED_ARG JSPropertyNameAccumulatorAddName(propertyNames, toJSString(\"${name}\"_s).get());\n" unless $hasDynamicProperties;

        my $condition = &$generateCondition($_);
        my $conditionalString = conditionalString($_);
        my $platformString = platformString($_);

        my $content = "";
        $content .= "#if ${platformString}\n" if $platformString;
        $content .= "#if ${conditionalString}\n" if $conditionalString;
        $content .= "    if (${condition})\n";
        $content .= "        SUPPRESS_UNCOUNTED_ARG JSPropertyNameAccumulatorAddName(propertyNames, toJSString(\"${name}\"_s).get());\n";
        $content .= "#endif // ${conditionalString}\n" if $conditionalString;
        $content .= "#endif // ${platformString}\n" if $platformString;
        $content .= "\n";
        return $content;
    };

    push(@contents, map(&$mapFunction, @{$interface->operations})) if @{$interface->operations};
    push(@contents, map(&$mapFunction, @{$interface->attributes})) if @{$interface->attributes};

    push(@contents, <<EOF);
}

bool ${className}::hasProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName)
{
    RefPtr impl = to${implementationClassName}(context, thisObject);
    if (!impl) [[unlikely]]
        return false;

    RefPtr page = toWebPage(context);
EOF

    push(@contents, "    bool isForMainWorld = impl->isForMainWorld();\n\n") if $hasMainWorldOnlyProperties;

    $mapFunction = sub {
        return unless $_->extendedAttributes->{"Dynamic"} or $_->extendedAttributes->{"MainWorldOnly"};

        my $name = $_->name;
        my $condition = &$generateCondition($_);
        my $conditionalString = conditionalString($_);
        my $platformString = platformString($_);

        my $content = "";
        $content .= "#if ${platformString}\n" if $platformString;
        $content .= "#if ${conditionalString}\n" if $conditionalString;
        $content .= "    if (JSStringIsEqualToUTF8CString(propertyName, \"${name}\"))\n";
        $content .= "        return ${condition};\n";
        $content .= "#endif // ${conditionalString}\n" if $conditionalString;
        $content .= "#endif // ${platformString}\n" if $platformString;
        $content .= "\n";
        return $content;
    };

    push(@contents, map(&$mapFunction, @{$interface->operations})) if @{$interface->operations};
    push(@contents, map(&$mapFunction, @{$interface->attributes})) if @{$interface->attributes};

    push(@contents, "    return false;\n");

    push(@contents, <<EOF);
}

JSValueRef ${className}::getProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    RefPtr impl = to${implementationClassName}(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RefPtr page = toWebPage(context);
EOF

    push(@contents, "    bool isForMainWorld = impl->isForMainWorld();\n") if $hasMainWorldOnlyProperties;
    push(@contents, "\n") if $hasMainWorldOnlyProperties and $hasDynamicProperties;

    if ($hasMainWorldOnlyProperties and !$hasDynamicProperties) {
        push(@contents, "    if (!isForMainWorld)\n");
        push(@contents, "        return JSValueMakeUndefined(context);\n\n");
    }

    if (my @attributes = @{$interface->attributes}) {
        foreach my $attribute (@attributes) {
            next unless $attribute->extendedAttributes->{"Dynamic"} or $attribute->extendedAttributes->{"MainWorldOnly"};

            my $name = $attribute->name;
            my $getterName = $self->_getterName($attribute);

            my $condition = "JSStringIsEqualToUTF8CString(propertyName, \"${name}\")" if $hasMainWorldOnlyProperties and !$hasDynamicProperties;
            $condition = &$generateCondition($attribute, "JSStringIsEqualToUTF8CString(propertyName, \"${name}\")") if $hasDynamicProperties;

            my $conditionalString = conditionalString($attribute);
            my $platformString = platformString($attribute);

            push(@contents, "#if ${platformString}\n") if $platformString;
            push(@contents, "#if ${conditionalString}\n") if $conditionalString;
            push(@contents, "    if (${condition})\n");
            push(@contents, "        return ${getterName}(context, thisObject, propertyName, exception);\n");
            push(@contents, "#endif // ${conditionalString}\n") if $conditionalString;
            push(@contents, "#endif // ${platformString}\n") if $platformString;
            push(@contents, "\n");
        }
    }

    if (my @functions = @{$interface->operations}) {
        foreach my $function (@functions) {
            next unless $function->extendedAttributes->{"Dynamic"} or $function->extendedAttributes->{"MainWorldOnly"};

            my $name = $function->name;

            my $condition = "JSStringIsEqualToUTF8CString(propertyName, \"${name}\")" if $hasMainWorldOnlyProperties and !$hasDynamicProperties;
            $condition = &$generateCondition($function, "JSStringIsEqualToUTF8CString(propertyName, \"${name}\")") if $hasDynamicProperties;

            my $conditionalString = conditionalString($function);

            push(@contents, "#if ${conditionalString}\n") if $conditionalString;
            push(@contents, "    if (${condition})\n");
            push(@contents, "        return JSObjectMakeFunctionWithCallback(context, propertyName, ${name});\n");
            push(@contents, "#endif // ${conditionalString}\n") if $conditionalString;
            push(@contents, "\n");
        }
    }

    push(@contents, <<EOF);
    return JSValueMakeUndefined(context);
}
EOF

    return @contents;
}

1;
