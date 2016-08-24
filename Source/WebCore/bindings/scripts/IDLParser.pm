# 
# KDOM IDL parser
#
# Copyright (C) 2005 Nikolas Zimmermann <wildfox@kde.org>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.
# 

package IDLParser;

use strict;

use Carp qw<longmess>;
use Data::Dumper;

use preprocessor;
use Class::Struct;

use constant StringToken => 0;
use constant IntegerToken => 1;
use constant FloatToken => 2;
use constant IdentifierToken => 3;
use constant OtherToken => 4;
use constant EmptyToken => 5;

# Used to represent a parsed IDL document
struct( idlDocument => {
    interfaces => '@', # List of 'domInterface'
    enumerations => '@', # List of 'domEnum'
    dictionaries => '@', # List of 'domDictionary'
    fileName => '$',
});

# Used to represent 'interface' blocks
struct( domInterface => {
    name => '$',      # Class identifier
    parent => '$',      # Parent class identifier
    parents => '@',      # Parent class identifiers (Kept for compatibility with ObjC bindings)
    constants => '@',    # List of 'domConstant'
    functions => '@',    # List of 'domFunction'
    anonymousFunctions => '@', # List of 'domFunction'
    attributes => '@',    # List of 'domAttribute'    
    extendedAttributes => '$', # Extended attributes
    constructors => '@', # Constructors, list of 'domFunction'
    customConstructors => '@', # Custom constructors, list of 'domFunction'
    isException => '$', # Used for exception interfaces
    isCallback => '$', # Used for callback interfaces
    isPartial => '$', # Used for partial interfaces
    iterable => '$', # Used for iterable interfaces
});

# Used to represent domInterface contents (name of method, signature)
struct( domFunction => {
    isStatic => '$',
    signature => '$',    # Return type/Object name/extended attributes
    parameters => '@',    # List of 'domSignature'
});

# Used to represent domInterface contents (name of attribute, signature)
struct( domAttribute => {
    type => '$',              # Attribute type (including namespace)
    isStatic => '$',
    isStringifier => '$',
    isReadOnly => '$',
    signature => '$',         # Attribute signature
});

struct( domType => {
    name =>         '$', # Type identifier
    isNullable =>   '$', # Is the type Nullable (T?)
    isUnion =>      '$', # Is the type a union (T or U)
    subtypes =>     '@', # Array of subtypes, only valid if isUnion or sequence
});

# Used to represent a map of 'variable name' <-> 'variable type'
struct( domSignature => {
    direction => '$',   # Variable direction (in or out)
    name => '$',        # Variable name
    type => '$'      ,  # Variable type
    specials => '@',    # Specials
    extendedAttributes => '$', # Extended attributes
    isNullable => '$',  # Is variable type Nullable (T?)
    isVariadic => '$',  # Is variable variadic (long... numbers)
    isOptional => '$',  # Is variable optional (optional T)
    default => '$',     # Default value for parameters
});

# Used to represent Iterable interfaces
struct( domIterable => {
    isKeyValue => '$',# Is map iterable or set iterable
    keyType => '$',   # Key type for map iterables
    valueType => '$', # Value type for map or set iterables
    functions => '@', # Iterable functions (entries, keys, values, [Symbol.Iterator], forEach)
    extendedAttributes => '$', # Extended attributes
});

# Used to represent string constants
struct( domConstant => {
    name => '$',        # DOM Constant identifier
    type => '$',        # Type of data
    value => '$',       # Constant value
    extendedAttributes => '$', # Extended attributes
});

# Used to represent 'enum' definitions
struct( domEnum => {
    name => '$', # Enumeration identifier
    values => '@', # Enumeration values (list of unique strings)
    extendedAttributes => '$',
});

struct( domDictionary => {
    name => '$',
    members => '@', # List of 'domSignature'
    extendedAttributes => '$',
});

struct( Token => {
    type => '$', # type of token
    value => '$' # value of token
});

struct( Typedef => {
    extendedAttributes => '$', # Extended attributes
    type => '$', # Type of data
});

# Maps 'typedef name' -> Typedef
my %typedefs = ();

sub new {
    my $class = shift;

    my $emptyToken = Token->new();
    $emptyToken->type(EmptyToken);
    $emptyToken->value("empty");

    my $self = {
        DocumentContent => "",
        EmptyToken => $emptyToken,
        NextToken => $emptyToken,
        Token => $emptyToken,
        Line => "",
        LineNumber => 1
    };
    return bless $self, $class;
}

sub assert
{
    my $message = shift;
    
    my $mess = longmess();
    print Dumper($mess);

    die $message;
}

sub assertTokenValue
{
    my $self = shift;
    my $token = shift;
    my $value = shift;
    my $line = shift;
    my $msg = "Next token should be " . $value . ", but " . $token->value() . " on line " . $self->{Line};
    if (defined ($line)) {
        $msg .= " IDLParser.pm:" . $line;
    }

    assert $msg unless $token->value() eq $value;
}

sub assertTokenType
{
    my $self = shift;
    my $token = shift;
    my $type = shift;
    
    assert "Next token's type should be " . $type . ", but " . $token->type() . " on line " . $self->{Line} unless $token->type() eq $type;
}

sub assertUnexpectedToken
{
    my $self = shift;
    my $token = shift;
    my $line = shift;
    my $msg = "Unexpected token " . $token . " on line " . $self->{Line};
    if (defined ($line)) {
        $msg .= " IDLParser.pm:" . $line;
    }

    assert $msg;
}

sub assertNoExtendedAttributesInTypedef
{
    my $self = shift;
    my $name = shift;
    my $line = shift;
    my $typedef = $typedefs{$name};
    my $msg = "Unexpected extendedAttributeList in typedef \"$name\" on line " . $self->{Line};
    if (defined ($line)) {
        $msg .= " IDLParser.pm:" . $line;
    }

    assert $msg if %{$typedef->extendedAttributes};
}

sub Parse
{
    my $self = shift;
    my $fileName = shift;
    my $defines = shift;
    my $preprocessor = shift;

    my @definitions = ();

    my @lines = applyPreprocessor($fileName, $defines, $preprocessor);
    $self->{Line} = $lines[0];
    $self->{DocumentContent} = join(' ', @lines);

    $self->getToken();
    eval {
        my $result = $self->parseDefinitions();
        push(@definitions, @{$result});

        my $next = $self->nextToken();
        $self->assertTokenType($next, EmptyToken);
    };
    assert $@ . " in $fileName" if $@;

    my $document = idlDocument->new();
    $document->fileName($fileName);
    foreach my $definition (@definitions) {
        if (ref($definition) eq "domInterface") {
            push(@{$document->interfaces}, $definition);
        } elsif (ref($definition) eq "domEnum") {
            push(@{$document->enumerations}, $definition);
        } elsif (ref($definition) eq "domDictionary") {
            push(@{$document->dictionaries}, $definition);
        } else {
            die "Unrecognized IDL definition kind: \"" . ref($definition) . "\"";
        }
    }
    return $document;
}

sub nextToken
{
    my $self = shift;
    return $self->{NextToken};
}

sub getToken
{
    my $self = shift;
    $self->{Token} = $self->{NextToken};
    $self->{NextToken} = $self->getTokenInternal();
    return $self->{Token};
}

my $whitespaceTokenPattern = '^[\t\n\r ]*[\n\r]';
my $floatTokenPattern = '^(-?(([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)([Ee][+-]?[0-9]+)?|[0-9]+[Ee][+-]?[0-9]+))';
my $integerTokenPattern = '^(-?[1-9][0-9]*|-?0[Xx][0-9A-Fa-f]+|-?0[0-7]*)';
my $stringTokenPattern = '^(\"[^\"]*\")';
my $identifierTokenPattern = '^([A-Z_a-z][0-9A-Z_a-z]*)';
my $otherTokenPattern = '^(\.\.\.|[^\t\n\r 0-9A-Z_a-z])';

sub getTokenInternal
{
    my $self = shift;

    if ($self->{DocumentContent} =~ /$whitespaceTokenPattern/) {
        $self->{DocumentContent} =~ s/($whitespaceTokenPattern)//;
        my $skipped = $1;
        $self->{LineNumber}++ while ($skipped =~ /\n/g);
        if ($self->{DocumentContent} =~ /^([^\n\r]+)/) {
            $self->{Line} = $self->{LineNumber} . ":" . $1;
        } else {
            $self->{Line} = "Unknown";
        }
    }
    $self->{DocumentContent} =~ s/^([\t\n\r ]+)//;
    if ($self->{DocumentContent} eq "") {
        return $self->{EmptyToken};
    }

    my $token = Token->new();
    if ($self->{DocumentContent} =~ /$floatTokenPattern/) {
        $token->type(FloatToken);
        $token->value($1);
        $self->{DocumentContent} =~ s/$floatTokenPattern//;
        return $token;
    }
    if ($self->{DocumentContent} =~ /$integerTokenPattern/) {
        $token->type(IntegerToken);
        $token->value($1);
        $self->{DocumentContent} =~ s/$integerTokenPattern//;
        return $token;
    }
    if ($self->{DocumentContent} =~ /$stringTokenPattern/) {
        $token->type(StringToken);
        $token->value($1);
        $self->{DocumentContent} =~ s/$stringTokenPattern//;
        return $token;
    }
    if ($self->{DocumentContent} =~ /$identifierTokenPattern/) {
        $token->type(IdentifierToken);
        $token->value($1);
        $self->{DocumentContent} =~ s/$identifierTokenPattern//;
        return $token;
    }
    if ($self->{DocumentContent} =~ /$otherTokenPattern/) {
        $token->type(OtherToken);
        $token->value($1);
        $self->{DocumentContent} =~ s/$otherTokenPattern//;
        return $token;
    }
    die "Failed in tokenizing at " . $self->{Line};
}

sub unquoteString
{
    my $self = shift;
    my $quotedString = shift;
    if ($quotedString =~ /^"([^"]*)"$/) {
        return $1;
    }
    die "Failed to parse string (" . $quotedString . ") at " . $self->{Line};
}

sub identifierRemoveNullablePrefix
{
    my $type = shift;
    $type =~ s/^_//;
    return $type;
}

my $nextAttribute_1 = '^(attribute|inherit|readonly)$';
my $nextPrimitiveType_1 = '^(int|long|short|unsigned)$';
my $nextPrimitiveType_2 = '^(double|float|unrestricted)$';
my $nextArgumentList_1 = '^(\(|ByteString|DOMString|USVString|Date|\[|any|boolean|byte|double|float|in|long|object|octet|optional|sequence|short|unrestricted|unsigned)$';
my $nextNonAnyType_1 = '^(boolean|byte|double|float|long|octet|short|unrestricted|unsigned)$';
my $nextInterfaceMember_1 = '^(\(|ByteString|DOMString|USVString|Date|any|attribute|boolean|byte|creator|deleter|double|float|getter|inherit|legacycaller|long|object|octet|readonly|sequence|serializer|setter|short|static|stringifier|unrestricted|unsigned|void)$';
my $nextAttributeOrOperation_1 = '^(static|stringifier)$';
my $nextAttributeOrOperation_2 = '^(\(|ByteString|DOMString|USVString|Date|any|boolean|byte|creator|deleter|double|float|getter|legacycaller|long|object|octet|sequence|setter|short|unrestricted|unsigned|void)$';
my $nextUnrestrictedFloatType_1 = '^(double|float)$';
my $nextExtendedAttributeRest3_1 = '^(\,|\])$';
my $nextExceptionField_1 = '^(\(|ByteString|DOMString|USVString|Date|any|boolean|byte|double|float|long|object|octet|sequence|short|unrestricted|unsigned)$';
my $nextType_1 = '^(ByteString|DOMString|USVString|Date|any|boolean|byte|double|float|long|object|octet|sequence|short|unrestricted|unsigned)$';
my $nextSpecials_1 = '^(creator|deleter|getter|legacycaller|setter)$';
my $nextDefinitions_1 = '^(callback|dictionary|enum|exception|interface|partial|typedef)$';
my $nextExceptionMembers_1 = '^(\(|ByteString|DOMString|USVString|Date|\[|any|boolean|byte|const|double|float|long|object|octet|optional|sequence|short|unrestricted|unsigned)$';
my $nextAttributeRest_1 = '^(attribute|readonly)$';
my $nextInterfaceMembers_1 = '^(\(|ByteString|DOMString|USVString|Date|any|attribute|boolean|byte|const|creator|deleter|double|float|getter|inherit|legacycaller|long|object|octet|readonly|sequence|serializer|setter|short|static|stringifier|unrestricted|unsigned|void)$';
my $nextSingleType_1 = '^(ByteString|DOMString|USVString|Date|boolean|byte|double|float|long|object|octet|sequence|short|unrestricted|unsigned)$';
my $nextArgumentName_1 = '^(attribute|callback|const|creator|deleter|dictionary|enum|exception|getter|implements|inherit|interface|legacycaller|partial|serializer|setter|static|stringifier|typedef|unrestricted)$';
my $nextConstValue_1 = '^(false|true)$';
my $nextConstValue_2 = '^(-|Infinity|NaN)$';
my $nextDefinition_1 = '^(callback|interface)$';
my $nextAttributeOrOperationRest_1 = '^(\(|ByteString|DOMString|USVString|Date|any|boolean|byte|double|float|long|object|octet|sequence|short|unrestricted|unsigned|void)$';
my $nextUnsignedIntegerType_1 = '^(long|short)$';
my $nextDefaultValue_1 = '^(-|Infinity|NaN|false|null|true)$';


sub parseDefinitions
{
    my $self = shift;
    my @definitions = ();

    while (1) {
        my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
        my $next = $self->nextToken();
        my $definition;
        if ($next->type() == IdentifierToken || $next->value() =~ /$nextDefinitions_1/) {
            $definition = $self->parseDefinition($extendedAttributeList);
        } else {
            last;
        }
        if (defined ($definition)) {
            push(@definitions, $definition);
        }
    }
    $self->applyTypedefs(\@definitions);
    return \@definitions;
}

sub applyTypedefs
{
    my $self = shift;
    my $definitions = shift;
   
    if (!%typedefs) {
        return;
    }
    foreach my $definition (@$definitions) {
        if (ref($definition) eq "domInterface") {
            foreach my $constant (@{$definition->constants}) {
                if (exists $typedefs{$constant->type}) {
                    my $typedef = $typedefs{$constant->type};
                    $self->assertNoExtendedAttributesInTypedef($constant->type, __LINE__);
                    $constant->type($typedef->type);
                }
            }
            foreach my $attribute (@{$definition->attributes}) {
                $self->applyTypedefsForSignature($attribute->signature);
            }
            foreach my $function (@{$definition->functions}, @{$definition->anonymousFunctions}, @{$definition->constructors}, @{$definition->customConstructors}) {
                $self->applyTypedefsForSignature($function->signature);
                foreach my $signature (@{$function->parameters}) {
                    $self->applyTypedefsForSignature($signature);
                }
            }
        } elsif (ref($definition) eq "domDictionary") {
            foreach my $member (@{$definition->members}) {
                $self->applyTypedefsForSignature($member);
            }
        }
    }
}

sub applyTypedefsForSignature
{
    my $self = shift;
    my $signature = shift;

    if (!defined ($signature->type)) {
        return;
    }

    my $type = $signature->type;
    $type =~ s/[\?\[\]]+$//g;
    my $typeSuffix = $signature->type;
    $typeSuffix =~ s/^[^\?\[\]]+//g;
    if (exists $typedefs{$type}) {
        my $typedef = $typedefs{$type};
        $signature->type($typedef->type . $typeSuffix);
        copyExtendedAttributes($signature->extendedAttributes, $typedef->extendedAttributes);
    }

    # Handle union types, sequences and etc.
    foreach my $name (%typedefs) {
        if (!exists $typedefs{$name}) {
            next;
        }
        my $typedef = $typedefs{$name};
        my $regex = '\\b' . $name . '\\b';
        my $replacement = $typedef->type;
        my $type = $signature->type;
        $type =~ s/($regex)/$replacement/g;
        $signature->type($type);
    }
}

sub parseDefinition
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() =~ /$nextDefinition_1/) {
        return $self->parseCallbackOrInterface($extendedAttributeList);
    }
    if ($next->value() eq "partial") {
        return $self->parsePartial($extendedAttributeList);
    }
    if ($next->value() eq "dictionary") {
        return $self->parseDictionary($extendedAttributeList);
    }
    if ($next->value() eq "exception") {
        return $self->parseException($extendedAttributeList);
    }
    if ($next->value() eq "enum") {
        return $self->parseEnum($extendedAttributeList);
    }
    if ($next->value() eq "typedef") {
        return $self->parseTypedef($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken) {
        return $self->parseImplementsStatement($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseCallbackOrInterface
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "callback") {
        $self->assertTokenValue($self->getToken(), "callback", __LINE__);
        return $self->parseCallbackRestOrInterface($extendedAttributeList);
    }
    if ($next->value() eq "interface") {
        return $self->parseInterface($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseCallbackRestOrInterface
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "interface") {
        my $interface = $self->parseInterface($extendedAttributeList);
        $interface->isCallback(1);
        return $interface;
    }
    if ($next->type() == IdentifierToken) {
        return $self->parseCallbackRest($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseInterface
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "interface") {
        my $interface = domInterface->new();
        $self->assertTokenValue($self->getToken(), "interface", __LINE__);
        my $interfaceNameToken = $self->getToken();
        $self->assertTokenType($interfaceNameToken, IdentifierToken);
        $interface->name(identifierRemoveNullablePrefix($interfaceNameToken->value()));
        my $parents = $self->parseInheritance();
        $interface->parents($parents);
        $interface->parent($parents->[0]);
        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        my $interfaceMembers = $self->parseInterfaceMembers();
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        applyMemberList($interface, $interfaceMembers);
        applyExtendedAttributeList($interface, $extendedAttributeList);
        return $interface;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parsePartial
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "partial") {
        $self->assertTokenValue($self->getToken(), "partial", __LINE__);
        return $self->parsePartialDefinition($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parsePartialDefinition
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "interface") {
        my $interface = $self->parseInterface($extendedAttributeList);
        $interface->isPartial(1);
        return $interface;
    }
    if ($next->value() eq "dictionary") {
        return $self->parsePartialDictionary($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parsePartialInterface
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "interface") {
        $self->assertTokenValue($self->getToken(), "interface", __LINE__);
        $self->assertTokenType($self->getToken(), IdentifierToken);
        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        $self->parseInterfaceMembers();
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseInterfaceMembers
{
    my $self = shift;
    my @interfaceMembers = ();

    while (1) {
        my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
        my $next = $self->nextToken();
        my $interfaceMember;

        if ($next->type() == IdentifierToken || $next->value() =~ /$nextInterfaceMembers_1/) {
            $interfaceMember = $self->parseInterfaceMember($extendedAttributeList);
        } else {
            last;
        }
        if (defined $interfaceMember) {
            push(@interfaceMembers, $interfaceMember);
        }
    }
    return \@interfaceMembers;
}

sub parseInterfaceMember
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "const") {
        return $self->parseConst($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextInterfaceMember_1/) {
        return $self->parseAttributeOrOperationOrIterator($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseDictionary
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "dictionary") {
        my $dictionary = domDictionary->new();
        $dictionary->extendedAttributes($extendedAttributeList);
        $self->assertTokenValue($self->getToken(), "dictionary", __LINE__);
        my $nameToken = $self->getToken();
        $self->assertTokenType($nameToken, IdentifierToken);
        $dictionary->name($nameToken->value());
        $self->parseInheritance();
        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        $dictionary->members($self->parseDictionaryMembers());
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return $dictionary;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseDictionaryMembers
{
    my $self = shift;

    my @members = ();

    while (1) {
        my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
        my $next = $self->nextToken();
        if ($next->type() == IdentifierToken || $next->value() =~ /$nextExceptionField_1/) {
            push(@members, $self->parseDictionaryMember($extendedAttributeList));
        } else {
            last;
        }
    }

    return \@members;
}

sub parseDictionaryMember
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextExceptionField_1/) {
        my $member = domSignature->new();
        if ($next->value ne "required") {
            $member->isOptional(1);
        } else {
            $self->assertTokenValue($self->getToken(), "required", __LINE__);
            $member->isOptional(0);
        }
        $member->extendedAttributes($extendedAttributeList);

        my $type = $self->parseType();
        $member->type($type->name);
        $member->isNullable($type->isNullable);

        my $nameToken = $self->getToken();
        $self->assertTokenType($nameToken, IdentifierToken);
        $member->name($nameToken->value);
        $member->default($self->parseDefault());
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return $member;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parsePartialDictionary
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "dictionary") {
        $self->assertTokenValue($self->getToken(), "dictionary", __LINE__);
        $self->assertTokenType($self->getToken(), IdentifierToken);
        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        $self->parseDictionaryMembers();
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseDefault
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "=") {
        $self->assertTokenValue($self->getToken(), "=", __LINE__);
        return $self->parseDefaultValue();
    }
    return undef;
}

sub parseDefaultValue
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->type() == FloatToken || $next->type() == IntegerToken || $next->value() =~ /$nextDefaultValue_1/) {
        return $self->parseConstValue();
    }
    if ($next->type() == StringToken) {
        return $self->getToken()->value();
    }
    if ($next->value() eq "[") {
        $self->assertTokenValue($self->getToken(), "[", __LINE__);
        $self->assertTokenValue($self->getToken(), "]", __LINE__);
        return "[]";
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseException
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "exception") {
        my $interface = domInterface->new();
        $self->assertTokenValue($self->getToken(), "exception", __LINE__);
        my $exceptionNameToken = $self->getToken();
        $self->assertTokenType($exceptionNameToken, IdentifierToken);
        $interface->name(identifierRemoveNullablePrefix($exceptionNameToken->value()));
        $interface->isException(1);
        my $parents = $self->parseInheritance();
        $interface->parents($parents);
        $interface->parent($parents->[0]);
        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        my $exceptionMembers = $self->parseExceptionMembers();
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        applyMemberList($interface, $exceptionMembers);
        applyExtendedAttributeList($interface, $extendedAttributeList);
        return $interface;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseExceptionMembers
{
    my $self = shift;
    my @members = ();

    while (1) {
        my $next = $self->nextToken();
        if ($next->type() == IdentifierToken || $next->value() =~ /$nextExceptionMembers_1/) {
            my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
            #my $member = $self->parseExceptionMember($extendedAttributeList);
            my $member = $self->parseInterfaceMember($extendedAttributeList);
            if (defined ($member)) {
                push(@members, $member);
            }
        } else {
            last;
        }
    }
    return \@members;
}

sub parseInheritance
{
    my $self = shift;
    my @parent = ();

    my $next = $self->nextToken();
    if ($next->value() eq ":") {
        $self->assertTokenValue($self->getToken(), ":", __LINE__);
        my $name = $self->parseName();
        push(@parent, $name);

        # FIXME: Remove. Was needed for needed for ObjC bindings.
        push(@parent, @{$self->parseIdentifiers()});
    }
    return \@parent;
}

sub parseEnum
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "enum") {
        my $enum = domEnum->new();
        $self->assertTokenValue($self->getToken(), "enum", __LINE__);
        my $enumNameToken = $self->getToken();
        $self->assertTokenType($enumNameToken, IdentifierToken);
        $enum->name(identifierRemoveNullablePrefix($enumNameToken->value()));
        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        push(@{$enum->values}, @{$self->parseEnumValueList()});
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        $enum->extendedAttributes($extendedAttributeList);
        return $enum;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseEnumValueList
{
    my $self = shift;
    my @values = ();
    my $next = $self->nextToken();
    if ($next->type() == StringToken) {
        my $enumValueToken = $self->getToken();
        $self->assertTokenType($enumValueToken, StringToken);
        my $enumValue = $self->unquoteString($enumValueToken->value());
        push(@values, $enumValue);
        push(@values, @{$self->parseEnumValues()});
        return \@values;
    }
    # value list must be non-empty
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseEnumValues
{
    my $self = shift;
    my @values = ();
    my $next = $self->nextToken();
    if ($next->value() eq ",") {
        $self->assertTokenValue($self->getToken(), ",", __LINE__);
        my $enumValueToken = $self->getToken();
        $self->assertTokenType($enumValueToken, StringToken);
        my $enumValue = $self->unquoteString($enumValueToken->value());
        push(@values, $enumValue);
        push(@values, @{$self->parseEnumValues()});
        return \@values;
    }
    return \@values; # empty list (end of enumeration-values)
}

sub parseCallbackRest
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        $self->assertTokenType($self->getToken(), IdentifierToken);
        $self->assertTokenValue($self->getToken(), "=", __LINE__);
        $self->parseReturnType();
        $self->assertTokenValue($self->getToken(), "(", __LINE__);
        $self->parseArgumentList();
        $self->assertTokenValue($self->getToken(), ")", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseTypedef
{
    my $self = shift;
    my $extendedAttributeList = shift;
    die "Extended attributes are not applicable to typedefs themselves: " . $self->{Line} if %{$extendedAttributeList};

    my $next = $self->nextToken();
    if ($next->value() eq "typedef") {
        $self->assertTokenValue($self->getToken(), "typedef", __LINE__);
        my $typedef = Typedef->new();
        $typedef->extendedAttributes($self->parseExtendedAttributeListAllowEmpty());

        my $type = $self->parseType();
        $typedef->type($type->name);

        my $nameToken = $self->getToken();
        $self->assertTokenType($nameToken, IdentifierToken);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        my $name = $nameToken->value();
        die "typedef redefinition for " . $name . " at " . $self->{Line} if (exists $typedefs{$name} && $typedef->type ne $typedefs{$name}->type);
        $typedefs{$name} = $typedef;
        return;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseImplementsStatement
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        $self->parseName();
        $self->assertTokenValue($self->getToken(), "implements", __LINE__);
        $self->parseName();
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseConst
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "const") {
        my $newDataNode = domConstant->new();
        $self->assertTokenValue($self->getToken(), "const", __LINE__);
        
        my $type = $self->parseConstType();
        $newDataNode->type($type->name);

        my $constNameToken = $self->getToken();
        $self->assertTokenType($constNameToken, IdentifierToken);
        $newDataNode->name(identifierRemoveNullablePrefix($constNameToken->value()));
        $self->assertTokenValue($self->getToken(), "=", __LINE__);
        $newDataNode->value($self->parseConstValue());
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        $newDataNode->extendedAttributes($extendedAttributeList);
        return $newDataNode;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseConstValue
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() =~ /$nextConstValue_1/) {
        return $self->parseBooleanLiteral();
    }
    if ($next->value() eq "null") {
        $self->assertTokenValue($self->getToken(), "null", __LINE__);
        return "null";
    }
    if ($next->type() == FloatToken || $next->value() =~ /$nextConstValue_2/) {
        return $self->parseFloatLiteral();
    }
    if ($next->type() == IntegerToken) {
        return $self->getToken()->value();
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseBooleanLiteral
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "true") {
        $self->assertTokenValue($self->getToken(), "true", __LINE__);
        return "true";
    }
    if ($next->value() eq "false") {
        $self->assertTokenValue($self->getToken(), "false", __LINE__);
        return "false";
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseFloatLiteral
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "-") {
        $self->assertTokenValue($self->getToken(), "-", __LINE__);
        $self->assertTokenValue($self->getToken(), "Infinity", __LINE__);
        return "-Infinity";
    }
    if ($next->value() eq "Infinity") {
        $self->assertTokenValue($self->getToken(), "Infinity", __LINE__);
        return "Infinity";
    }
    if ($next->value() eq "NaN") {
        $self->assertTokenValue($self->getToken(), "NaN", __LINE__);
        return "NaN";
    }
    if ($next->type() == FloatToken) {
        return $self->getToken()->value();
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseAttributeOrOperationOrIterator
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "serializer") {
        return $self->parseSerializer($extendedAttributeList);
    }
    if ($next->value() =~ /$nextAttributeOrOperation_1/) {
        my $qualifier = $self->parseQualifier();
        my $newDataNode = $self->parseAttributeOrOperationRest($extendedAttributeList);
        if (defined($newDataNode)) {
            $newDataNode->isStatic(1) if $qualifier eq "static";
            $newDataNode->isStringifier(1) if $qualifier eq "stringifier";
        }
        return $newDataNode;
    }
    if ($next->value() =~ /$nextAttribute_1/) {
        return $self->parseAttribute($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextAttributeOrOperation_2/) {
        return $self->parseOperationOrIterator($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseSerializer
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "serializer") {
        $self->assertTokenValue($self->getToken(), "serializer", __LINE__);
        return $self->parseSerializerRest($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseSerializerRest
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "=") {
        $self->assertTokenValue($self->getToken(), "=", __LINE__);
        return $self->parseSerializationPattern($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken || $next->value() eq "(") {
        return $self->parseOperationRest($extendedAttributeList);
    }
}

sub parseSerializationPattern
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "{") {
        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        $self->parseSerializationPatternMap();
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        return;
    }
    if ($next->value() eq "[") {
        $self->assertTokenValue($self->getToken(), "[", __LINE__);
        $self->parseSerializationPatternList();
        $self->assertTokenValue($self->getToken(), "]", __LINE__);
        return;
    }
    if ($next->type() == IdentifierToken) {
        $self->assertTokenType($self->getToken(), IdentifierToken);
        return;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseSerializationPatternMap
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "getter") {
        $self->assertTokenValue($self->getToken(), "getter", __LINE__);
        return;
    }
    if ($next->value() eq "inherit") {
        $self->assertTokenValue($self->getToken(), "inherit", __LINE__);
        $self->parseIdentifiers();
        return;
    }
    if ($next->type() == IdentifierToken) {
        $self->assertTokenType($self->getToken(), IdentifierToken);
        $self->parseIdentifiers();
    }
}

sub parseSerializationPatternList
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "getter") {
        $self->assertTokenValue($self->getToken(), "getter", __LINE__);
        return;
    }
    if ($next->type() == IdentifierToken) {
        $self->assertTokenType($self->getToken(), IdentifierToken);
        $self->parseIdentifiers();
    }
}

sub parseIdentifierList
{
    my $self = shift;
    my $next = $self->nextToken();

    my @identifiers = ();
    if ($next->type == IdentifierToken) {
        push(@identifiers, $self->getToken()->value());
        push(@identifiers, @{$self->parseIdentifiers()});
    }
    return @identifiers;
}

sub parseIdentifiers
{
    my $self = shift;
    my @idents = ();

    while (1) {
        my $next = $self->nextToken();
        if ($next->value() eq ",") {
            $self->assertTokenValue($self->getToken(), ",", __LINE__);
            my $token = $self->getToken();
            $self->assertTokenType($token, IdentifierToken);
            push(@idents, $token->value());
        } else {
            last;
        }
    }
    return \@idents;
}

sub parseQualifier
{
    my $self = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "static") {
        $self->assertTokenValue($self->getToken(), "static", __LINE__);
        return "static";
    }
    if ($next->value() eq "stringifier") {
        $self->assertTokenValue($self->getToken(), "stringifier", __LINE__);
        return "stringifier";
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseAttributeOrOperationRest
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() =~ /$nextAttributeRest_1/) {
        return $self->parseAttributeRest($extendedAttributeList);
    }
    if ($next->value() eq ";") {
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextAttributeOrOperationRest_1/) {
        my $returnType = $self->parseReturnType();
        my $interface = $self->parseOperationRest($extendedAttributeList);
        if (defined ($interface)) {
            $interface->signature->type($returnType->name);
            $interface->signature->isNullable($returnType->isNullable);
        }
        return $interface;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseAttribute
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() =~ /$nextAttribute_1/) {
        $self->parseInherit();
        return $self->parseAttributeRest($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseAttributeRest
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() =~ /$nextAttributeRest_1/) {
        my $newDataNode = domAttribute->new();
        if ($self->parseReadOnly()) {
            $newDataNode->type("attribute");
            $newDataNode->isReadOnly(1);
        } else {
            $newDataNode->type("attribute");
        }
        $self->assertTokenValue($self->getToken(), "attribute", __LINE__);
        $newDataNode->signature(domSignature->new());
        
        my $type = $self->parseType();
        $newDataNode->signature->type($type->name);
        $newDataNode->signature->isNullable($type->isNullable);

        my $token = $self->getToken();
        $self->assertTokenType($token, IdentifierToken);
        $newDataNode->signature->name(identifierRemoveNullablePrefix($token->value()));
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        # CustomConstructor may also be used on attributes.
        if (defined $extendedAttributeList->{"CustomConstructors"}) {
            delete $extendedAttributeList->{"CustomConstructors"};
            $extendedAttributeList->{"CustomConstructor"} = "VALUE_IS_MISSING";
        }
        $newDataNode->signature->extendedAttributes($extendedAttributeList);
        return $newDataNode;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseInherit
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "inherit") {
        $self->assertTokenValue($self->getToken(), "inherit", __LINE__);
        return 1;
    }
    return 0;
}

sub parseReadOnly
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "readonly") {
        $self->assertTokenValue($self->getToken(), "readonly", __LINE__);
        return 1;
    }
    return 0;
}

sub parseOperationOrIterator
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() =~ /$nextSpecials_1/) {
        return $self->parseSpecialOperation($extendedAttributeList);
    }
    if ($next->value() eq "iterable") {
        return $self->parseIterableRest($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextAttributeOrOperationRest_1/) {
        my $returnType = $self->parseReturnType();
        my $next = $self->nextToken();
        if ($next->type() == IdentifierToken || $next->value() eq "(") {
            my $operation = $self->parseOperationRest($extendedAttributeList);
            $operation->signature->type($returnType->name);
            $operation->signature->isNullable($returnType->isNullable);

            return $operation;
        }
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseSpecialOperation
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() =~ /$nextSpecials_1/) {
        my @specials = ();
        push(@specials, @{$self->parseSpecials()});
        my $returnType = $self->parseReturnType();
        my $interface = $self->parseOperationRest($extendedAttributeList);
        if (defined ($interface)) {
            $interface->signature->type($returnType->name);
            $interface->signature->isNullable($returnType->isNullable);
            $interface->signature->specials(\@specials);
        }
        return $interface;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseSpecials
{
    my $self = shift;
    my @specials = ();

    while (1) {
        my $next = $self->nextToken();
        if ($next->value() =~ /$nextSpecials_1/) {
            push(@specials, $self->parseSpecial());
        } else {
            last;
        }
    }
    return \@specials;
}

sub parseSpecial
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "getter") {
        $self->assertTokenValue($self->getToken(), "getter", __LINE__);
        return "getter";
    }
    if ($next->value() eq "setter") {
        $self->assertTokenValue($self->getToken(), "setter", __LINE__);
        return "setter";
    }
    if ($next->value() eq "creator") {
        $self->assertTokenValue($self->getToken(), "creator", __LINE__);
        return "creator";
    }
    if ($next->value() eq "deleter") {
        $self->assertTokenValue($self->getToken(), "deleter", __LINE__);
        return "deleter";
    }
    if ($next->value() eq "legacycaller") {
        $self->assertTokenValue($self->getToken(), "legacycaller", __LINE__);
        return "legacycaller";
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseIterableRest
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "iterable") {
        $self->assertTokenValue($self->getToken(), "iterable", __LINE__);
        my $iterableNode = $self->parseOptionalIterableInterface($extendedAttributeList);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return $iterableNode;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseOptionalIterableInterface
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $symbolIteratorFunction = domFunction->new();
    $symbolIteratorFunction->signature(domSignature->new());
    $symbolIteratorFunction->signature->extendedAttributes($extendedAttributeList);
    $symbolIteratorFunction->signature->name("[Symbol.Iterator]");

    my $entriesFunction = domFunction->new();
    $entriesFunction->signature(domSignature->new());
    $entriesFunction->signature->extendedAttributes($extendedAttributeList);
    $entriesFunction->signature->name("entries");

    my $keysFunction = domFunction->new();
    $keysFunction->signature(domSignature->new());
    $keysFunction->signature->extendedAttributes($extendedAttributeList);
    $keysFunction->signature->name("keys");

    my $valuesFunction = domFunction->new();
    $valuesFunction->signature(domSignature->new());
    $valuesFunction->signature->extendedAttributes($extendedAttributeList);
    $valuesFunction->signature->name("values");

    my $forEachFunction = domFunction->new();
    $forEachFunction->signature(domSignature->new());
    $forEachFunction->signature->extendedAttributes($extendedAttributeList);
    $forEachFunction->signature->name("forEach");
    my $forEachArgument = domSignature->new();
    $forEachArgument->name("callback");
    $forEachArgument->type("any");
    push(@{$forEachFunction->parameters}, ($forEachArgument));

    my $newDataNode = domIterable->new();
    $newDataNode->extendedAttributes($extendedAttributeList);
    push(@{$newDataNode->functions}, $symbolIteratorFunction);
    push(@{$newDataNode->functions}, $entriesFunction);
    push(@{$newDataNode->functions}, $keysFunction);
    push(@{$newDataNode->functions}, $valuesFunction);
    push(@{$newDataNode->functions}, $forEachFunction);

    $self->assertTokenValue($self->getToken(), "<", __LINE__);
    my $type1 = $self->getToken()->value();
    if ($self->nextToken()->value() eq ",") {
        $self->assertTokenValue($self->getToken(), ",", __LINE__);
        $newDataNode->isKeyValue(1);
        $newDataNode->keyType($type1);
        $newDataNode->valueType($self->getToken()->value());
    } else {
        $newDataNode->isKeyValue(0);
        $newDataNode->valueType($type1);
    }
    $self->assertTokenValue($self->getToken(), ">", __LINE__);

    return $newDataNode;
}

sub parseOperationRest
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken || $next->value() eq "(") {
        my $newDataNode = domFunction->new();
        $newDataNode->signature(domSignature->new());
        my $name = $self->parseOptionalIdentifier();
        $newDataNode->signature->name(identifierRemoveNullablePrefix($name));
        $self->assertTokenValue($self->getToken(), "(", $name, __LINE__);
        push(@{$newDataNode->parameters}, @{$self->parseArgumentList()});
        $self->assertTokenValue($self->getToken(), ")", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        $newDataNode->signature->extendedAttributes($extendedAttributeList);
        return $newDataNode;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseOptionalIdentifier
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        my $token = $self->getToken();
        return $token->value();
    }
    return "";
}

sub parseArgumentList
{
    my $self = shift;
    my @arguments = ();

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextArgumentList_1/) {
        push(@arguments, $self->parseArgument());
        push(@arguments, @{$self->parseArguments()});
    }
    return \@arguments;
}

sub parseArguments
{
    my $self = shift;
    my @arguments = ();

    while (1) {
        my $next = $self->nextToken();
        if ($next->value() eq ",") {
            $self->assertTokenValue($self->getToken(), ",", __LINE__);
            push(@arguments, $self->parseArgument());
        } else {
            last;
        }
    }
    return \@arguments;
}

sub parseArgument
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextArgumentList_1/) {
        my $in = $self->parseIn();
        my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
        my $argument = $self->parseOptionalOrRequiredArgument($extendedAttributeList);
        $argument->direction($self->parseIn());
        return $argument;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseOptionalOrRequiredArgument
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $paramDataNode = domSignature->new();
    $paramDataNode->extendedAttributes($extendedAttributeList);

    my $next = $self->nextToken();
    if ($next->value() eq "optional") {
        $self->assertTokenValue($self->getToken(), "optional", __LINE__);

        my $type = $self->parseType();
        $paramDataNode->type(identifierRemoveNullablePrefix($type->name));
        $paramDataNode->isNullable($type->isNullable);
        $paramDataNode->isOptional(1);
        $paramDataNode->name($self->parseArgumentName());
        $paramDataNode->default($self->parseDefault());
        return $paramDataNode;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextExceptionField_1/) {
        my $type = $self->parseType();
        $paramDataNode->type($type->name);
        $paramDataNode->isNullable($type->isNullable);
        $paramDataNode->isOptional(0);
        $paramDataNode->isVariadic($self->parseEllipsis());
        $paramDataNode->name($self->parseArgumentName());
        return $paramDataNode;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseArgumentName
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() =~ /$nextArgumentName_1/) {
        return $self->parseArgumentNameKeyword();
    }
    if ($next->type() == IdentifierToken) {
        return $self->getToken()->value();
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseEllipsis
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "...") {
        $self->assertTokenValue($self->getToken(), "...", __LINE__);
        return 1;
    }
    return 0;
}

sub parseExceptionMember
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "const") {
        return $self->parseConst($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextExceptionField_1/) {
        return $self->parseExceptionField($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseExceptionField
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextExceptionField_1/) {
        my $newDataNode = domAttribute->new();
        $newDataNode->type("attribute");
        $newDataNode->isReadOnly(1);
        $newDataNode->signature(domSignature->new());

        my $type = $self->parseType();
        $newDataNode->signature->type($type->name);
        $newDataNode->signature->isNullable($type->isNullable);
        
        my $token = $self->getToken();
        $self->assertTokenType($token, IdentifierToken);
        $newDataNode->signature->name(identifierRemoveNullablePrefix($token->value()));
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        $newDataNode->signature->extendedAttributes($extendedAttributeList);
        return $newDataNode;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseExtendedAttributeListAllowEmpty
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "[") {
        return $self->parseExtendedAttributeList();
    }
    return {};
}

sub copyExtendedAttributes
{
    my $extendedAttributeList = shift;
    my $attr = shift;

    for my $key (keys %{$attr}) {
        if ($key eq "Constructor") {
            push(@{$extendedAttributeList->{"Constructors"}}, $attr->{$key});
        } elsif ($key eq "Constructors") {
            my @constructors = @{$attr->{$key}};
            foreach my $constructor (@constructors) {
                push(@{$extendedAttributeList->{"Constructors"}}, $constructor);
            }
        } elsif ($key eq "CustomConstructor") {
            push(@{$extendedAttributeList->{"CustomConstructors"}}, $attr->{$key});
        } elsif ($key eq "CustomConstructors") {
           my @customConstructors = @{$attr->{$key}};
            foreach my $customConstructor (@customConstructors) {
                push(@{$extendedAttributeList->{"CustomConstructors"}}, $customConstructor);
            }
        } else {
            $extendedAttributeList->{$key} = $attr->{$key};
        }
    }
}

sub parseExtendedAttributeList
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "[") {
        $self->assertTokenValue($self->getToken(), "[", __LINE__);
        my $extendedAttributeList = {};
        my $attr = $self->parseExtendedAttribute();
        copyExtendedAttributes($extendedAttributeList, $attr);
        $attr = $self->parseExtendedAttributes();
        copyExtendedAttributes($extendedAttributeList, $attr);
        $self->assertTokenValue($self->getToken(), "]", __LINE__);
        return $extendedAttributeList;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseExtendedAttributes
{
    my $self = shift;
    my $extendedAttributeList = {};

    while (1) {
        my $next = $self->nextToken();
        if ($next->value() eq ",") {
            $self->assertTokenValue($self->getToken(), ",", __LINE__);
            my $attr = $self->parseExtendedAttribute2();
            copyExtendedAttributes($extendedAttributeList, $attr);
        } else {
            last;
        }
    }
    return $extendedAttributeList;
}

sub parseExtendedAttribute
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        my $name = $self->parseName();
        return $self->parseExtendedAttributeRest($name);
    }
    # backward compatibility. Spec doesn' allow "[]". But WebKit requires.
    if ($next->value() eq ']') {
        return {};
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseExtendedAttribute2
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        my $name = $self->parseName();
        return $self->parseExtendedAttributeRest($name);
    }
    return {};
}

sub parseExtendedAttributeRest
{
    my $self = shift;
    my $name = shift;
    my $attrs = {};

    my $next = $self->nextToken();
    if ($next->value() eq "(") {
        $self->assertTokenValue($self->getToken(), "(", __LINE__);
        $attrs->{$name} = $self->parseArgumentList();
        $self->assertTokenValue($self->getToken(), ")", __LINE__);
        return $attrs;
    }
    if ($next->value() eq "=") {
        $self->assertTokenValue($self->getToken(), "=", __LINE__);
        $attrs->{$name} = $self->parseExtendedAttributeRest2();
        return $attrs;
    }

    if ($name eq "Constructor" || $name eq "CustomConstructor") {
        $attrs->{$name} = [];
    } else {
        $attrs->{$name} = "VALUE_IS_MISSING";
    }
    return $attrs;
}

sub parseExtendedAttributeRest2
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "(") {
        $self->assertTokenValue($self->getToken(), "(", __LINE__);
        my @arguments = $self->parseIdentifierList();
        $self->assertTokenValue($self->getToken(), ")", __LINE__);
        return @arguments;
    }
    if ($next->type() == IdentifierToken) {
        my $name = $self->parseName();
        return $self->parseExtendedAttributeRest3($name);
    }
    if ($next->type() == IntegerToken) {
        my $token = $self->getToken();
        return $token->value();
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseExtendedAttributeRest3
{
    my $self = shift;
    my $name = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "&") {
        $self->assertTokenValue($self->getToken(), "&", __LINE__);
        my $rightValue = $self->parseName();
        return $name . "&" . $rightValue;
    }
    if ($next->value() eq "|") {
        $self->assertTokenValue($self->getToken(), "|", __LINE__);
        my $rightValue = $self->parseName();
        return $name . "|" . $rightValue;
    }
    if ($next->value() eq "(") {
        my $attr = {};
        $self->assertTokenValue($self->getToken(), "(", __LINE__);
        $attr->{$name} = $self->parseArgumentList();
        $self->assertTokenValue($self->getToken(), ")", __LINE__);
        return $attr;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextExtendedAttributeRest3_1/) {
        $self->parseNameNoComma();
        return $name;
    }
    $self->assertUnexpectedToken($next->value());
}

sub parseArgumentNameKeyword
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "attribute") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "callback") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "const") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "creator") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "deleter") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "dictionary") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "enum") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "exception") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "getter") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "implements") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "inherit") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "interface") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "legacycaller") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "partial") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "serializer") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "setter") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "static") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "stringifier") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "typedef") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "unrestricted") {
        return $self->getToken()->value();
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "(") {
        return $self->parseUnionType();
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextType_1/) {
        return $self->parseSingleType();
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseSingleType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "any") {
        $self->assertTokenValue($self->getToken(), "any", __LINE__);
        
        my $anyType = domType->new();
        $anyType->name("any");
        return $anyType;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextSingleType_1/) {
        my $nonAnyType = $self->parseNonAnyType();
        $nonAnyType->isNullable($self->parseNull());
        return $nonAnyType;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseUnionType
{
    my $self = shift;
    my $next = $self->nextToken();

    my $unionType = domType->new();
    $unionType->name("UNION");
    $unionType->isUnion(1);

    if ($next->value() eq "(") {
        $self->assertTokenValue($self->getToken(), "(", __LINE__);
        
        push(@{$unionType->subtypes}, $self->parseUnionMemberType());
        
        $self->assertTokenValue($self->getToken(), "or", __LINE__);
        
        push(@{$unionType->subtypes}, $self->parseUnionMemberType());
        push(@{$unionType->subtypes}, $self->parseUnionMemberTypes());
        
        $self->assertTokenValue($self->getToken(), ")", __LINE__);

        return $unionType;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseUnionMemberType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "(") {
        my $unionType = $self->parseUnionType();
        $unionType->isNullable($self->parseNull());
        return $unionType;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextSingleType_1/) {
        my $nonAnyType = $self->parseNonAnyType();
        $nonAnyType->isNullable($self->parseNull());
        return $nonAnyType;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseUnionMemberTypes
{
    my $self = shift;
    my $next = $self->nextToken();

    my @subtypes = ();

    if ($next->value() eq "or") {
        $self->assertTokenValue($self->getToken(), "or", __LINE__);
        push(@subtypes, $self->parseUnionMemberType());
        push(@subtypes, $self->parseUnionMemberTypes());
    }

    return @subtypes;
}

sub parseNonAnyType
{
    my $self = shift;
    my $next = $self->nextToken();

    my $type = domType->new();

    if ($next->value() =~ /$nextNonAnyType_1/) {
        $type->name($self->parsePrimitiveType());
        return $type;
    }
    if ($next->value() eq "ByteString") {
        $self->assertTokenValue($self->getToken(), "ByteString", __LINE__);

        $type->name("ByteString");
        return $type;
    }
    if ($next->value() eq "DOMString") {
        $self->assertTokenValue($self->getToken(), "DOMString", __LINE__);

        $type->name("DOMString");
        return $type;
    }
    if ($next->value() eq "USVString") {
        $self->assertTokenValue($self->getToken(), "USVString", __LINE__);

        $type->name("USVString");
        return $type;
    }
    if ($next->value() eq "object") {
        $self->assertTokenValue($self->getToken(), "object", __LINE__);

        $type->name("object");
        return $type;
    }
    if ($next->value() eq "RegExp") {
        $self->assertTokenValue($self->getToken(), "RegExp", __LINE__);

        $type->name("RegExp");
        return $type;
    }
    if ($next->value() eq "Error") {
        $self->assertTokenValue($self->getToken(), "Error", __LINE__);

        $type->name("Error");
        return $type;
    }
    if ($next->value() eq "DOMException") {
        $self->assertTokenValue($self->getToken(), "DOMException", __LINE__);

        $type->name("DOMException");
        return $type;
    }
    if ($next->value() eq "Date") {
        $self->assertTokenValue($self->getToken(), "Date", __LINE__);

        $type->name("Date");
        return $type;
    }
    if ($next->value() eq "sequence") {
        $self->assertTokenValue($self->getToken(), "sequence", __LINE__);
        $self->assertTokenValue($self->getToken(), "<", __LINE__);

        my $subtype = $self->parseType();
        my $subtypeName = $subtype->name;

        $self->assertTokenValue($self->getToken(), ">", __LINE__);

        # FIXME: This should just be "sequence" when we start using domTypes in the CodeGenerators
        $type->name("sequence<${subtypeName}>");
        push(@{$type->subtypes}, $subtype);

        return $type;
    }
    if ($next->value() eq "FrozenArray") {
        $self->assertTokenValue($self->getToken(), "FrozenArray", __LINE__);
        $self->assertTokenValue($self->getToken(), "<", __LINE__);

        my $subtype = $self->parseType();
        my $subtypeName = $subtype->name;

        $self->assertTokenValue($self->getToken(), ">", __LINE__);

        # FIXME: This should just be "FrozenArray" when we start using domTypes in the CodeGenerators
        $type->name("FrozenArray<${subtypeName}>");
        push(@{$type->subtypes}, $subtype);

        return $type;
    }
    if ($next->type() == IdentifierToken) {
        my $identifier = $self->getToken();

        $type->name(identifierRemoveNullablePrefix($identifier->value()));
        return $type;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseConstType
{
    my $self = shift;
    my $next = $self->nextToken();

    my $type = domType->new();

    if ($next->value() =~ /$nextNonAnyType_1/) {
        $type->name($self->parsePrimitiveType());
        $type->isNullable($self->parseNull());
        return $type;
    }
    if ($next->type() == IdentifierToken) {
        my $identifier = $self->getToken();
        
        $type->name($identifier->value());
        $type->isNullable($self->parseNull());

        return $type;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parsePrimitiveType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() =~ /$nextPrimitiveType_1/) {
        return $self->parseUnsignedIntegerType();
    }
    if ($next->value() =~ /$nextPrimitiveType_2/) {
        return $self->parseUnrestrictedFloatType();
    }
    if ($next->value() eq "boolean") {
        $self->assertTokenValue($self->getToken(), "boolean", __LINE__);
        return "boolean";
    }
    if ($next->value() eq "byte") {
        $self->assertTokenValue($self->getToken(), "byte", __LINE__);
        return "byte";
    }
    if ($next->value() eq "octet") {
        $self->assertTokenValue($self->getToken(), "octet", __LINE__);
        return "octet";
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseUnrestrictedFloatType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "unrestricted") {
        $self->assertTokenValue($self->getToken(), "unrestricted", __LINE__);
        return "unrestricted " . $self->parseFloatType();
    }
    if ($next->value() =~ /$nextUnrestrictedFloatType_1/) {
        return $self->parseFloatType();
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseFloatType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "float") {
        $self->assertTokenValue($self->getToken(), "float", __LINE__);
        return "float";
    }
    if ($next->value() eq "double") {
        $self->assertTokenValue($self->getToken(), "double", __LINE__);
        return "double";
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseUnsignedIntegerType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "unsigned") {
        $self->assertTokenValue($self->getToken(), "unsigned", __LINE__);
        return "unsigned " . $self->parseIntegerType();
    }
    if ($next->value() =~ /$nextUnsignedIntegerType_1/) {
        return $self->parseIntegerType();
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseIntegerType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "short") {
        $self->assertTokenValue($self->getToken(), "short", __LINE__);
        return "short";
    }
    if ($next->value() eq "int") {
        $self->assertTokenValue($self->getToken(), "int", __LINE__);
        return "int";
    }
    if ($next->value() eq "long") {
        $self->assertTokenValue($self->getToken(), "long", __LINE__);
        if ($self->parseOptionalLong()) {
            return "long long";
        }
        return "long";
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseOptionalLong
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "long") {
        $self->assertTokenValue($self->getToken(), "long", __LINE__);
        return 1;
    }
    return 0;
}

sub parseNull
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "?") {
        $self->assertTokenValue($self->getToken(), "?", __LINE__);
        return 1;
    }
    return 0;
}

sub parseReturnType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "void") {
        $self->assertTokenValue($self->getToken(), "void", __LINE__);
        
        my $voidType = domType->new();
        $voidType->name("void");
        return $voidType;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextExceptionField_1/) {
        return $self->parseType();
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseIn
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "in") {
        $self->assertTokenValue($self->getToken(), "in", __LINE__);
        return "in";
    }
    return "";
}

sub parseOptionalSemicolon
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq ";") {
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
    }
}

sub parseNameNoComma
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        my $identifier = $self->getToken();
        return ($identifier->value());
    }

    return ();
}

sub parseName
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        my $identifier = $self->getToken();
        return $identifier->value();
    }
    $self->assertUnexpectedToken($next->value());
}


sub applyMemberList
{
    my $interface = shift;
    my $members = shift;

    for my $item (@{$members}) {
        if (ref($item) eq "domAttribute") {
            push(@{$interface->attributes}, $item);
            next;
        }
        if (ref($item) eq "domConstant") {
            push(@{$interface->constants}, $item);
            next;
        }
        if (ref($item) eq "domIterable") {
            $interface->iterable($item);
            next;
        }
        if (ref($item) eq "domFunction") {
            if ($item->signature->name eq "") {
                push(@{$interface->anonymousFunctions}, $item);
            } else {
                push(@{$interface->functions}, $item);
            }
            next;
        }
    }
}

sub applyExtendedAttributeList
{
    my $interface = shift;
    my $extendedAttributeList = shift;

    if (defined $extendedAttributeList->{"Constructors"}) {
        my @constructorParams = @{$extendedAttributeList->{"Constructors"}};
        my $index = (@constructorParams == 1) ? 0 : 1;
        foreach my $param (@constructorParams) {
            my $constructor = domFunction->new();
            $constructor->signature(domSignature->new());
            $constructor->signature->name("Constructor");
            $constructor->signature->extendedAttributes($extendedAttributeList);
            $constructor->parameters($param);
            push(@{$interface->constructors}, $constructor);
        }
        delete $extendedAttributeList->{"Constructors"};
        $extendedAttributeList->{"Constructor"} = "VALUE_IS_MISSING";
    } elsif (defined $extendedAttributeList->{"NamedConstructor"}) {
        my $newDataNode = domFunction->new();
        $newDataNode->signature(domSignature->new());
        $newDataNode->signature->name("NamedConstructor");
        $newDataNode->signature->extendedAttributes($extendedAttributeList);
        my %attributes = %{$extendedAttributeList->{"NamedConstructor"}};
        my @attributeKeys = keys (%attributes);
        my $constructorName = $attributeKeys[0];
        push(@{$newDataNode->parameters}, @{$attributes{$constructorName}});
        $extendedAttributeList->{"NamedConstructor"} = $constructorName;
        push(@{$interface->constructors}, $newDataNode);
    }
    if (defined $extendedAttributeList->{"CustomConstructors"}) {
        my @customConstructorParams = @{$extendedAttributeList->{"CustomConstructors"}};
        my $index = (@customConstructorParams == 1) ? 0 : 1;
        foreach my $param (@customConstructorParams) {
            my $customConstructor = domFunction->new();
            $customConstructor->signature(domSignature->new());
            $customConstructor->signature->name("CustomConstructor");
            $customConstructor->signature->extendedAttributes($extendedAttributeList);
            $customConstructor->parameters($param);
            push(@{$interface->customConstructors}, $customConstructor);
        }
        delete $extendedAttributeList->{"CustomConstructors"};
        $extendedAttributeList->{"CustomConstructor"} = "VALUE_IS_MISSING";
    }
    $interface->extendedAttributes($extendedAttributeList);
}

1;

