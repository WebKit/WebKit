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
struct( IDLDocument => {
    interfaces => '@', # List of 'IDLInterface'
    enumerations => '@', # List of 'IDLEnum'
    dictionaries => '@', # List of 'IDLDictionary'
    callbackFunctions => '@', # List of 'IDLCallbackFunction'
    namespaces => '@', # List of 'IDLNamespace'
    includes => '@', # List of 'IDLIncludesStatement'
    fileName => '$',
});

# https://heycam.github.io/webidl/#idl-types
struct( IDLType => {
    name =>         '$', # Type identifier
    isNullable =>   '$', # Is the type Nullable (T?)
    isUnion =>      '$', # Is the type a union (T or U)
    subtypes =>     '@', # Array of subtypes, only valid if isUnion or sequence
    extendedAttributes => '%',
});

# Used to represent 'interface' blocks
struct( IDLInterface => {
    type => 'IDLType',
    parentType => 'IDLType',
    constants => '@',    # List of 'IDLConstant'
    operations => '@',    # List of 'IDLOperation'
    anonymousOperations => '@', # List of 'IDLOperation'
    attributes => '@',    # List of 'IDLAttribute'
    constructors => '@', # Constructors, list of 'IDLOperation'
    isCallback => '$', # Used for callback interfaces
    isMixin => '$', # Used for mixin interfaces
    isPartial => '$', # Used for partial interfaces
    iterable => '$', # Used for iterable interfaces, of type 'IDLIterable'
    asyncIterable => '$', # Used for asycn iterable interfaces, of type 'IDLAsyncIterable'
    mapLike => '$', # Used for mapLike interfaces, of type 'IDLMapLike'
    setLike => '$', # Used for setLike interfaces, of type 'IDLSetLike'
    serializable => '$', # Used for serializable interfaces, of type 'IDLSerializable'
    extendedAttributes => '$',
});

# Used to represent an argument to a IDLOperation.
struct( IDLArgument => {
    name => '$',
    type => 'IDLType',
    isVariadic => '$',
    isOptional => '$',
    default => '$',
    extendedAttributes => '%',
});

# https://heycam.github.io/webidl/#idl-operations
struct( IDLOperation => {
    name => '$',
    type => 'IDLType', # Return type
    arguments => '@', # List of 'IDLArgument'
    isConstructor => '$',
    isStatic => '$',
    isIterable => '$',
    isSerializer => '$',
    isStringifier => '$',
    isMapLike => '$',
    isSetLike => '$',
    specials => '@',
    extendedAttributes => '%',
});


# https://heycam.github.io/webidl/#idl-attributes
struct( IDLAttribute => {
    name => '$',
    type => 'IDLType',
    isConstructor => '$',
    isStatic => '$',
    isMapLike => '$',
    isSetLike => '$',
    isStringifier => '$',
    isReadOnly => '$',
    isInherit => '$',
    extendedAttributes => '$',
});

# https://heycam.github.io/webidl/#idl-iterable
struct( IDLIterable => {
    isKeyValue => '$',
    keyType => 'IDLType',
    valueType => 'IDLType',
    operations => '@', # Iterable operations (entries, keys, values, [Symbol.Iterator], forEach)
    extendedAttributes => '$',
});

# https://heycam.github.io/webidl/#idl-async-iterable
struct( IDLAsyncIterable => {
    isKeyValue => '$',
    keyType => 'IDLType',
    valueType => 'IDLType',
    arguments => '@', # List of 'IDLArgument'
    extendedAttributes => '$',
});

# https://heycam.github.io/webidl/#es-maplike
struct( IDLMapLike => {
    isReadOnly => '$',
    keyType => 'IDLType',
    valueType => 'IDLType',
    attributes => '@', # MapLike attributes (size)
    operations => '@', # MapLike operations (entries, keys, values, forEach, get, has and if not readonly, delete, set and clear)
    extendedAttributes => '$',
});

# https://heycam.github.io/webidl/#es-setlike
struct( IDLSetLike => {
    isReadOnly => '$',
    itemType => 'IDLType',
    attributes => '@', # SetLike attributes (size)
    operations => '@', # SetLike operations (entries, keys, values, forEach, has and if not readonly, delete, set and clear)
    extendedAttributes => '$',
});

# https://heycam.github.io/webidl/#idl-serializers
struct( IDLSerializable => {
    attributes => '@', # List of attributes to serialize
    hasAttribute => '$', # serializer = { attribute }
    hasInherit => '$', # serializer = { inherit }
    hasGetter => '$', # serializer = { getter }
    operations => '@', # toJSON operation
});

# https://heycam.github.io/webidl/#idl-constants
struct( IDLConstant => {
    name => '$',
    type => 'IDLType',
    value => '$',
    extendedAttributes => '$',
});

# https://heycam.github.io/webidl/#idl-enums
struct( IDLEnum => {
    name => '$',
    type => 'IDLType',
    values => '@',
    extendedAttributes => '$',
});

# https://heycam.github.io/webidl/#dfn-dictionary-member
struct( IDLDictionaryMember => {
    name => '$',
    type => 'IDLType',
    isRequired => '$',
    default => '$',
    extendedAttributes => '$',
});

# https://heycam.github.io/webidl/#idl-dictionaries
struct( IDLDictionary => {
    type => 'IDLType',
    parentType => 'IDLType',
    members => '@', # List of 'IDLDictionaryMember'
    extendedAttributes => '$',
    isPartial => '$', # Used for partial dictionaries
});

# https://heycam.github.io/webidl/#idl-callback-functions
struct( IDLCallbackFunction => {
    type => '$',
    operation => 'IDLOperation',
    extendedAttributes => '$',
});

# https://heycam.github.io/webidl/#idl-namespaces
struct( IDLNamespace => {
    name => '$',
    operations => '@', # List of 'IDLOperation'
    attributes => '@', # List of 'IDLAttribute'
    extendedAttributes => '$',
    isPartial => '$', # Used for partial namespaces
});

# https://heycam.github.io/webidl/#includes-statement
struct( IDLIncludesStatement => {
    interfaceIdentifier => '$',
    mixinIdentifier => '$',
});

# https://heycam.github.io/webidl/#idl-typedefs
struct( IDLTypedef => {
    type => 'IDLType',
});

struct( Token => {
    type => '$', # type of token
    value => '$' # value of token
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
        LineNumber => 1,
        ExtendedAttributeMap => ""
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

sub assertExtendedAttributesValidForContext
{
    my $self = shift;
    my $extendedAttributeList = shift;
    my @contexts = @_;

    for my $extendedAttribute (keys %{$extendedAttributeList}) {
        # FIXME: Should this be done here, or when parsing the exteded attribute itself?
        # Either way, we should add validation of the values, if any, at the same place.

        if (!exists $self->{ExtendedAttributeMap}->{$extendedAttribute}) {
            assert "Unknown extended attribute: '${extendedAttribute}'";
        }

        my $foundAllowedContext = 0;
        for my $contextAllowed (@{$self->{ExtendedAttributeMap}->{$extendedAttribute}->{"contextsAllowed"}}) {
            for my $context (@contexts) {
                if ($contextAllowed eq $context) {
                    $foundAllowedContext = 1;
                    last;
                }
            }
        }

        if (!$foundAllowedContext) {
            if (scalar(@contexts) == 1) {
                assert "Extended attribute '${extendedAttribute}' used in invalid context '" . $contexts[0] . "'";
            } else {
                # FIXME: Improved this error message a bit.
                assert "Extended attribute '${extendedAttribute}' used in invalid context";
            }
        }
    }
}

sub Parse
{
    my $self = shift;
    my $fileName = shift;
    my $defines = shift;
    my $preprocessor = shift;
    my $idlAttributes = shift;

    my @definitions = ();

    my @lines = applyPreprocessor($fileName, $defines, $preprocessor);
    $self->{Line} = $lines[0];
    $self->{DocumentContent} = join(' ', @lines);
    $self->{ExtendedAttributeMap} = $idlAttributes;

    addBuiltinTypedefs();

    $self->getToken();
    eval {
        my $result = $self->parseDefinitions();
        push(@definitions, @{$result});

        my $next = $self->nextToken();
        $self->assertTokenType($next, EmptyToken);
    };
    assert $@ . " in $fileName" if $@;

    my $document = IDLDocument->new();
    $document->fileName($fileName);
    foreach my $definition (@definitions) {
        if (ref($definition) eq "IDLInterface") {
            push(@{$document->interfaces}, $definition);
        } elsif (ref($definition) eq "IDLEnum") {
            push(@{$document->enumerations}, $definition);
        } elsif (ref($definition) eq "IDLDictionary") {
            push(@{$document->dictionaries}, $definition);
        } elsif (ref($definition) eq "IDLCallbackFunction") {
            push(@{$document->callbackFunctions}, $definition);
        } elsif (ref($definition) eq "IDLNamespace") {
            push(@{$document->namespaces}, $definition);
        } elsif (ref($definition) eq "IDLIncludesStatement") {
            push(@{$document->includes}, $definition);
        } else {
            die "Unrecognized IDL definition kind: \"" . ref($definition) . "\"";
        }
    }
    return $document;
}

sub ParseType
{
    my ($self, $type, $idlAttributes) = @_;

    $self->{Line} = $type;
    $self->{DocumentContent} = $type;
    $self->{ExtendedAttributeMap} = $idlAttributes;

    addBuiltinTypedefs();

    my $result;

    $self->getToken();
    eval {
        $result = $self->parseType();

        my $next = $self->nextToken();
        $self->assertTokenType($next, EmptyToken);
    };
    assert $@ . " parsing type ${type}" if $@;

    return $result;
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

sub copyExtendedAttributes
{
    my $extendedAttributeList = shift;
    my $attr = shift;

    for my $key (keys %{$attr}) {
        $extendedAttributeList->{$key} = $attr->{$key};
    }
}

sub isExtendedAttributeApplicableToTypes
{
    my $self = shift;
    my $extendedAttribute = shift;

    if (!exists $self->{ExtendedAttributeMap}->{$extendedAttribute}) {
        assert "Unknown extended attribute: '${extendedAttribute}'";
    }

    for my $contextAllowed (@{$self->{ExtendedAttributeMap}->{$extendedAttribute}->{"contextsAllowed"}}) {
        if ($contextAllowed eq "type") {
            return 1;
        }
    }

    return 0;
}

sub moveExtendedAttributesApplicableToTypes
{
    my $self = shift;
    my $type = shift;
    my $extendedAttributeList = shift;

    for my $key (keys %{$extendedAttributeList}) {
        if ($self->isExtendedAttributeApplicableToTypes($key)) {
            if (!defined $type->extendedAttributes->{$key}) {
                $type->extendedAttributes->{$key} = $extendedAttributeList->{$key};
            }
            delete $extendedAttributeList->{$key};
        }
    }
}

sub typeDescription
{
    my $type = shift;

    if (scalar @{$type->subtypes}) {
        return $type->name . '<' . join(', ', map { typeDescription($_) } @{$type->subtypes}) . '>' . ($type->isNullable ? "?" : "");
    }

    return $type->name . ($type->isNullable ? "?" : "");
}

sub cloneType
{
    my $type = shift;

    my $clonedType = IDLType->new();
    $clonedType->name($type->name);
    $clonedType->isNullable($type->isNullable);
    $clonedType->isUnion($type->isUnion);

    copyExtendedAttributes($clonedType->extendedAttributes, $type->extendedAttributes);

    foreach my $subtype (@{$type->subtypes}) {
        push(@{$clonedType->subtypes}, cloneType($subtype));
    }

    return $clonedType;
}

sub cloneArgument
{
    my $argument = shift;

    my $clonedArgument = IDLArgument->new();
    $clonedArgument->name($argument->name);
    $clonedArgument->type(cloneType($argument->type));
    $clonedArgument->isVariadic($argument->isVariadic);
    $clonedArgument->isOptional($argument->isOptional);
    $clonedArgument->default($argument->default);
    copyExtendedAttributes($clonedArgument->extendedAttributes, $argument->extendedAttributes);

    return $clonedArgument;
}

sub cloneOperation
{
    my $operation = shift;

    my $clonedOperation = IDLOperation->new();
    $clonedOperation->name($operation->name);
    $clonedOperation->type(cloneType($operation->type));
    
    foreach my $argument (@{$operation->arguments}) {
        push(@{$clonedOperation->arguments}, cloneArgument($argument));
    }

    $clonedOperation->isConstructor($operation->isConstructor);
    $clonedOperation->isStatic($operation->isStatic);
    $clonedOperation->isIterable($operation->isIterable);
    $clonedOperation->isSerializer($operation->isSerializer);
    $clonedOperation->isStringifier($operation->isStringifier);
    $clonedOperation->isMapLike($operation->isMapLike);
    $clonedOperation->isSetLike($operation->isSetLike);
    $clonedOperation->specials($operation->specials);

    copyExtendedAttributes($clonedOperation->extendedAttributes, $operation->extendedAttributes);

    return $clonedOperation;
}

sub makeSimpleType
{
    my $typeName = shift;

    return IDLType->new(name => $typeName);
}

sub addBuiltinTypedefs()
{
    # NOTE: This leaves out the ArrayBufferView definition as it is
    # treated as its own type, and not a union, to allow us to utilize
    # the shared base class all the members of the union have.

    # typedef (ArrayBufferView or ArrayBuffer) BufferSource;

    my $bufferSourceType = IDLType->new(name => "UNION", isUnion => 1);
    push(@{$bufferSourceType->subtypes}, makeSimpleType("ArrayBufferView"));
    push(@{$bufferSourceType->subtypes}, makeSimpleType("ArrayBuffer"));
    $typedefs{"BufferSource"} = IDLTypedef->new(type => $bufferSourceType);

    # typedef unsigned long long DOMTimeStamp;

    my $DOMTimeStampType = IDLType->new(name => "unsigned long long");
    $typedefs{"DOMTimeStamp"} = IDLTypedef->new(type => $DOMTimeStampType);
}

my $nextOptionallyReadonlyAttribute_1 = '^(readonly|attribute)$';
my $nextIntegerType_1 = '^(int|long|short|unsigned)$';
my $nextFloatType_1 = '^(double|float|unrestricted)$';
my $nextArgumentList_1 = '^(\(|ByteString|DOMString|USVString|\[|any|boolean|byte|double|float|in|long|object|octet|optional|sequence|short|symbol|undefined|unrestricted|unsigned)$';
my $nextPrimitiveType_1 = '^(boolean|byte|double|float|long|octet|short|symbol|undefined|unrestricted|unsigned)$';
my $nextStringType_1 = '^(ByteString|DOMString|USVString)$';
my $nextInterfaceMember_1 = '^(\(|ByteString|DOMString|USVString|any|attribute|boolean|byte|deleter|double|float|getter|inherit|long|object|octet|readonly|sequence|setter|short|symbol|undefined|unrestricted|unsigned)$';
my $nextOperation_1 = '^(\(|ByteString|DOMString|USVString|any|boolean|byte|deleter|double|float|getter|long|object|octet|sequence|setter|short|symbol|undefined|unrestricted|unsigned)$';
my $nextUnrestrictedFloatType_1 = '^(double|float)$';
my $nextExtendedAttributeRest3_1 = '^(\,|\])$';
my $nextType_1 = '^(ByteString|DOMString|USVString|any|boolean|byte|double|float|long|object|octet|sequence|short|symbol|undefined|unrestricted|unsigned)$';
my $nextSpecials_1 = '^(deleter|getter|setter)$';
my $nextDefinitions_1 = '^(callback|dictionary|enum|interface|namespace|partial|typedef)$';
my $nextDictionaryMember_1 = '^(\(|ByteString|DOMString|USVString|any|boolean|byte|double|float|long|object|octet|required|sequence|short|symbol|undefined|unrestricted|unsigned)$';
my $nextCallbackInterfaceMembers_1 = '^(\(|const|ByteString|DOMString|USVString|any|boolean|byte|double|float|long|object|octet|sequence|short|symbol|undefined|unrestricted|unsigned)$';
my $nextInterfaceMembers_1 = '^(\(|ByteString|DOMString|USVString|any|attribute|boolean|byte|const|constructor|deleter|double|float|getter|inherit|long|object|octet|readonly|sequence|serializer|setter|short|static|stringifier|symbol|undefined|unrestricted|unsigned)$';
my $nextMixinMembers_1 = '^(\(|attribute|ByteString|DOMString|USVString|any|boolean|byte|const|double|float|long|object|octet|readonly|sequence|short|stringifier|symbol|undefined|unrestricted|unsigned)$';
my $nextNamespaceMembers_1 = '^(\(|ByteString|DOMString|USVString|any|boolean|byte|double|float|long|object|octet|readonly|sequence|short|symbol|undefined|unrestricted|unsigned)$';
my $nextPartialInterfaceMember_1 = '^(\(|ByteString|DOMString|USVString|any|attribute|boolean|byte|const|deleter|double|float|getter|inherit|long|object|octet|readonly|sequence|serializer|setter|short|static|stringifier|symbol|undefined|unrestricted|unsigned)$';
my $nextSingleType_1 = '^(ByteString|DOMString|USVString|boolean|byte|double|float|long|object|octet|sequence|short|symbol|undefined|unrestricted|unsigned)$';
my $nextArgumentName_1 = '^(async|attribute|callback|const|constructor|deleter|dictionary|enum|getter|includes|inherit|interface|iterable|maplike|mixin|namespace|partial|readonly|required|setlike|setter|static|stringifier|typedef|unrestricted)$';
my $nextConstValue_1 = '^(false|true)$';
my $nextConstValue_2 = '^(-|Infinity|NaN)$';
my $nextCallbackOrInterface = '^(callback|interface)$';
my $nextRegularOperation_1 = '^(\(|ByteString|DOMString|USVString|any|boolean|byte|double|float|long|object|octet|sequence|short|symbol|undefined|unrestricted|unsigned)$';
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
        if (ref($definition) eq "IDLInterface") {
            foreach my $constant (@{$definition->constants}) {
                $constant->type($self->typeByApplyingTypedefs($constant->type));
            }
            foreach my $attribute (@{$definition->attributes}) {
                $attribute->type($self->typeByApplyingTypedefs($attribute->type));
            }
            foreach my $operation (@{$definition->operations}, @{$definition->anonymousOperations}, @{$definition->constructors}) {
                $self->applyTypedefsToOperation($operation);
            }
            if ($definition->iterable) {
                if ($definition->iterable->keyType) {
                    $definition->iterable->keyType($self->typeByApplyingTypedefs($definition->iterable->keyType));
                }
                if ($definition->iterable->valueType) {
                    $definition->iterable->valueType($self->typeByApplyingTypedefs($definition->iterable->valueType));
                }
                foreach my $operation (@{$definition->iterable->operations}) {
                    $self->applyTypedefsToOperation($operation);
                }
            }
            if ($definition->asyncIterable) {
                if ($definition->asyncIterable->keyType) {
                    $definition->asyncIterable->keyType($self->typeByApplyingTypedefs($definition->asyncIterable->keyType));
                }
                if ($definition->asyncIterable->valueType) {
                    $definition->asyncIterable->valueType($self->typeByApplyingTypedefs($definition->asyncIterable->valueType));
                }
                foreach my $argument (@{$definition->asyncIterable->arguments}) {
                    $argument->type($self->typeByApplyingTypedefs($argument->type));
                }
            }
            if ($definition->mapLike) {
                if ($definition->mapLike->keyType) {
                    $definition->mapLike->keyType($self->typeByApplyingTypedefs($definition->mapLike->keyType));
                }
                if ($definition->mapLike->valueType) {
                    $definition->mapLike->valueType($self->typeByApplyingTypedefs($definition->mapLike->valueType));
                }
                foreach my $attribute (@{$definition->mapLike->attributes}) {
                    $attribute->type($self->typeByApplyingTypedefs($attribute->type));
                }
                foreach my $operation (@{$definition->mapLike->operations}) {
                    $self->applyTypedefsToOperation($operation);
                }
            }
            if ($definition->setLike) {
                if ($definition->setLike->itemType) {
                    $definition->setLike->itemType($self->typeByApplyingTypedefs($definition->setLike->itemType));
                }
                foreach my $attribute (@{$definition->setLike->attributes}) {
                    $attribute->type($self->typeByApplyingTypedefs($attribute->type));
                }
                foreach my $operation (@{$definition->setLike->operations}) {
                    $self->applyTypedefsToOperation($operation);
                }
            }
        } elsif (ref($definition) eq "IDLDictionary") {
            foreach my $member (@{$definition->members}) {
                $member->type($self->typeByApplyingTypedefs($member->type));
            }
        } elsif (ref($definition) eq "IDLCallbackFunction") {
            $self->applyTypedefsToOperation($definition->operation);
        } elsif (ref($definition) eq "IDLNamespace") {
            foreach my $operation (@{$definition->operations}) {
                $self->applyTypedefsToOperation($operation);
            }
            foreach my $attribute (@{$definition->attributes}) {
                $attribute->type($self->typeByApplyingTypedefs($attribute->type));
            }
        }
    }
}

sub applyTypedefsToOperation
{
    my $self = shift;
    my $operation = shift;

    if ($operation->type) {
        $operation->type($self->typeByApplyingTypedefs($operation->type));
    }

    foreach my $argument (@{$operation->arguments}) {
        $argument->type($self->typeByApplyingTypedefs($argument->type));
    }
}

sub typeByApplyingTypedefs
{
    my $self = shift;
    my $type = shift;

    assert("Missing type") if !$type;

    my $numberOfSubtypes = scalar @{$type->subtypes};
    if ($numberOfSubtypes) {
        for my $i (0..$numberOfSubtypes - 1) {
            my $subtype = @{$type->subtypes}[$i];
            my $replacementSubtype = $self->typeByApplyingTypedefs($subtype);
            @{$type->subtypes}[$i] = $replacementSubtype
        }

        return $type;
    }

    if (exists $typedefs{$type->name}) {
        my $typedef = $typedefs{$type->name};

        my $clonedType = cloneType($typedef->type);
        $clonedType->isNullable($clonedType->isNullable || $type->isNullable);
        $self->moveExtendedAttributesApplicableToTypes($clonedType, $type->extendedAttributes);

        return $self->typeByApplyingTypedefs($clonedType);
    }
    
    return $type;
}

sub parseDefinition
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() =~ /$nextCallbackOrInterface/) {
        return $self->parseCallbackOrInterfaceOrMixin($extendedAttributeList);
    }
    if ($next->value() eq "namespace") {
        return $self->parseNamespace($extendedAttributeList);
    }
    if ($next->value() eq "partial") {
        return $self->parsePartial($extendedAttributeList);
    }
    if ($next->value() eq "dictionary") {
        return $self->parseDictionary($extendedAttributeList);
    }
    if ($next->value() eq "enum") {
        return $self->parseEnum($extendedAttributeList);
    }
    if ($next->value() eq "typedef") {
        return $self->parseTypedef($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken) {
        return $self->parseIncludesStatement($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseCallbackOrInterfaceOrMixin
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "callback") {
        $self->assertTokenValue($self->getToken(), "callback", __LINE__);
        return $self->parseCallbackRestOrInterface($extendedAttributeList);
    }
    if ($next->value() eq "interface") {
        $self->assertTokenValue($self->getToken(), "interface", __LINE__);
        return $self->parseInterfaceOrMixin($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseCallbackRestOrInterface
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "interface") {
        $self->assertTokenValue($self->getToken(), "interface", __LINE__);
        return $self->parseCallbackInterface($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken) {
        return $self->parseCallbackRest($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseInterfaceOrMixin
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "mixin") {
        return $self->parseMixin($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken) {
        return $self->parseInterface($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseInterface
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        my $interface = IDLInterface->new();

        my $interfaceNameToken = $self->getToken();
        $self->assertTokenType($interfaceNameToken, IdentifierToken);
        
        my $name = identifierRemoveNullablePrefix($interfaceNameToken->value());
        $interface->type(makeSimpleType($name));

        $next = $self->nextToken();
        if ($next->value() eq ":") {
            my $parent = $self->parseInheritance();
            $interface->parentType(makeSimpleType($parent));
        }

        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        my $interfaceMembers = $self->parseInterfaceMembers();
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        applyMemberList($interface, $interfaceMembers);

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "interface");
        applyExtendedAttributeList($interface, $extendedAttributeList);

        return $interface;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseCallbackInterface
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        my $interface = IDLInterface->new();
        $interface->isCallback(1);

        my $interfaceNameToken = $self->getToken();
        $self->assertTokenType($interfaceNameToken, IdentifierToken);
        
        my $name = identifierRemoveNullablePrefix($interfaceNameToken->value());
        $interface->type(makeSimpleType($name));

        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        my $interfaceMembers = $self->parseCallbackInterfaceMembers();
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        applyMemberList($interface, $interfaceMembers);

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "interface");
        applyExtendedAttributeList($interface, $extendedAttributeList);

        return $interface;
    }

    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseCallbackInterfaceMembers
{
    my $self = shift;
    my @callbackInterfaceMembers = ();

    while (1) {
        my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
        my $next = $self->nextToken();
        my $callbackInterfaceMember;

        if ($next->type() == IdentifierToken || $next->value() =~ /$nextCallbackInterfaceMembers_1/) {
            $callbackInterfaceMember = $self->parseCallbackInterfaceMember($extendedAttributeList);
        } else {
            last;
        }
        if (defined $callbackInterfaceMember) {
            push(@callbackInterfaceMembers, $callbackInterfaceMember);
        }
    }
    return \@callbackInterfaceMembers;
}

sub parseCallbackInterfaceMember
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "const") {
        return $self->parseConst($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextRegularOperation_1/) {
        return $self->parseRegularOperation($extendedAttributeList);
    }

    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseMixin
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "mixin") {
        $self->assertTokenValue($self->getToken(), "mixin", __LINE__);

        my $interfaceMixin = IDLInterface->new();
        $interfaceMixin->isMixin(1);

        my $interfaceNameToken = $self->getToken();
        $self->assertTokenType($interfaceNameToken, IdentifierToken);
        
        my $name = identifierRemoveNullablePrefix($interfaceNameToken->value());
        $interfaceMixin->type(makeSimpleType($name));

        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        my $interfaceMixinMembers = $self->parseMixinMembers();
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        applyMemberList($interfaceMixin, $interfaceMixinMembers);

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "interface");
        applyExtendedAttributeList($interfaceMixin, $extendedAttributeList);

        return $interfaceMixin;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseMixinMembers
{
    my $self = shift;
    my @mixinMembers = ();

    while (1) {
        my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
        my $next = $self->nextToken();
        my $mixinMember;

        if ($next->type() == IdentifierToken || $next->value() =~ /$nextMixinMembers_1/) {
            $mixinMember = $self->parseMixinMember($extendedAttributeList);
        } else {
            last;
        }
        if (defined $mixinMember) {
            push(@mixinMembers, $mixinMember);
        }
    }
    return \@mixinMembers;
}

sub parseMixinMember
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "const") {
        return $self->parseConst($extendedAttributeList);
    }
    if ($next->value() eq "stringifier") {
        return $self->parseStringifier($extendedAttributeList);
    }
    if ($next->value() =~ /$nextOptionallyReadonlyAttribute_1/) {
        my $isReadOnly = $self->parseReadOnly();
        my $attribute = $self->parseAttributeRest($extendedAttributeList);
        $attribute->isReadOnly($isReadOnly);
        return $attribute;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextRegularOperation_1/) {
        return $self->parseRegularOperation($extendedAttributeList);
    }

    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseNamespace
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "namespace") {
        $self->assertTokenValue($self->getToken(), "namespace", __LINE__);

        my $namespace = IDLNamespace->new();

        my $namespaceNameToken = $self->getToken();
        $self->assertTokenType($namespaceNameToken, IdentifierToken);
        
        my $name = identifierRemoveNullablePrefix($namespaceNameToken->value());
        $namespace->name($name);

        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        my $namespaceMembers = $self->parseNamespaceMembers();
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        
        for my $namespaceMember (@{$namespaceMembers}) {
            if (ref($namespaceMember) eq "IDLAttribute") {
                push(@{$namespace->attributes}, $namespaceMember);
                next;
            }
            if (ref($namespaceMember) eq "IDLOperation") {
                push(@{$namespace->operations}, $namespaceMember);
                next;
            }
        }

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "namespace");
        $namespace->extendedAttributes($extendedAttributeList);

        die "'namespace' definitions are not currently supported by the code generators.";

        return $namespace;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseNamespaceMembers
{
    my $self = shift;
    my @namespaceMembers = ();

    while (1) {
        my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
        my $next = $self->nextToken();
        my $namespaceMember;

        if ($next->type() == IdentifierToken || $next->value() =~ /$nextNamespaceMembers_1/) {
            $namespaceMember = $self->parseNamespaceMember($extendedAttributeList);
        } else {
            last;
        }
        if (defined $namespaceMember) {
            push(@namespaceMembers, $namespaceMember);
        }
    }
    return \@namespaceMembers;
}

sub parseNamespaceMember
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "readonly") {
        $self->assertTokenValue($self->getToken(), "readonly", __LINE__);
        my $attribute = $self->parseAttributeRest($extendedAttributeList);
        $attribute->isReadOnly(1);
        return $attribute;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextRegularOperation_1/) {
        return $self->parseRegularOperation($extendedAttributeList);
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
        $self->assertTokenValue($self->getToken(), "interface", __LINE__);
        return $self->parsePartialInterfaceOrPartialMixin($extendedAttributeList);
    }
    if ($next->value() eq "dictionary") {
        $self->assertTokenValue($self->getToken(), "dictionary", __LINE__);
        return $self->parsePartialDictionary($extendedAttributeList);
    }
    if ($next->value() eq "namespace") {
        my $namespace = $self->parseNamespace($extendedAttributeList);
        $namespace->isPartial(1);
        return $namespace;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parsePartialInterfaceOrPartialMixin
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "mixin") {
        my $mixin = $self->parseMixin($extendedAttributeList);
        $mixin->isPartial(1);
        return $mixin;
    }
    if ($next->type() == IdentifierToken) {
        return $self->parsePartialInterface($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parsePartialInterface
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        my $interface = IDLInterface->new();
        $interface->isPartial(1);

        my $interfaceNameToken = $self->getToken();
        $self->assertTokenType($interfaceNameToken, IdentifierToken);

        my $name = identifierRemoveNullablePrefix($interfaceNameToken->value());
        $interface->type(makeSimpleType($name));

        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        my $partialInterfaceMembers = $self->parsePartialInterfaceMembers();
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        applyMemberList($interface, $partialInterfaceMembers);

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "interface");
        applyExtendedAttributeList($interface, $extendedAttributeList);

        return $interface;
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

sub parsePartialInterfaceMembers
{
    my $self = shift;
    my @interfaceMembers = ();

    while (1) {
        my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
        my $next = $self->nextToken();
        my $interfaceMember;

        if ($next->type() == IdentifierToken || $next->value() =~ /$nextPartialInterfaceMember_1/) {
            $interfaceMember = $self->parsePartialInterfaceMember($extendedAttributeList);
        } else {
            last;
        }
        if (defined $interfaceMember) {
            push(@interfaceMembers, $interfaceMember);
        }
    }
    return \@interfaceMembers;
}

sub parsePartialInterfaceMember
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();

    if ($next->value() eq "const") {
        return $self->parseConst($extendedAttributeList);
    }
    if ($next->value() eq "stringifier") {
        return $self->parseStringifier($extendedAttributeList);
    }
    if ($next->value() eq "static") {
        return $self->parseStaticMember($extendedAttributeList);
    }
    if ($next->value() eq "iterable") {
        return $self->parseIterableRest($extendedAttributeList);
    }
    if ($next->value() eq "async") {
        return $self->parseAsyncIterable($extendedAttributeList);
    }
    if ($next->value() eq "readonly") {
        return $self->parseReadOnlyMember($extendedAttributeList);
    }
    if ($next->value() eq "attribute") {
        return $self->parseAttributeRest($extendedAttributeList);
    }
    if ($next->value() eq "maplike") {
        return $self->parseMapLikeRest($extendedAttributeList, 0);
    }
    if ($next->value() eq "setlike") {
        return $self->parseSetLikeRest($extendedAttributeList, 0);
    }
    if ($next->value() eq "inherit") {
        return $self->parseInheritAttribute($extendedAttributeList);
    }

    # FIXME: Remove. This is no longer part of WebIDL. Replace with [Serializable].
    if ($next->value() eq "serializer") {
        return $self->parseSerializer($extendedAttributeList);
    }

    if ($next->type() == IdentifierToken || $next->value() =~ /$nextOperation_1/) {
        return $self->parseOperation($extendedAttributeList);
    }

    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseInterfaceMember
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "constructor") {
        return $self->parseConstructor($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextPartialInterfaceMember_1/) {
        return $self->parsePartialInterfaceMember($extendedAttributeList);
    }

    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parsePartialDictionary
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        my $dictionary = IDLDictionary->new();
        $dictionary->isPartial(1);

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "dictionary");
        $dictionary->extendedAttributes($extendedAttributeList);

        my $nameToken = $self->getToken();
        $self->assertTokenType($nameToken, IdentifierToken);

        my $name = $nameToken->value();
        $dictionary->type(makeSimpleType($name));

        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        $dictionary->members($self->parseDictionaryMembers());
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return $dictionary;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseDictionary
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "dictionary") {
        $self->assertTokenValue($self->getToken(), "dictionary", __LINE__);

        my $dictionary = IDLDictionary->new();

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "dictionary");
        $dictionary->extendedAttributes($extendedAttributeList);

        my $nameToken = $self->getToken();
        $self->assertTokenType($nameToken, IdentifierToken);

        my $name = $nameToken->value();
        $dictionary->type(makeSimpleType($name));

        $next = $self->nextToken();
        if ($next->value() eq ":") {
            my $parent = $self->parseInheritance();
            $dictionary->parentType(makeSimpleType($parent));
        }
        
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
        if ($next->type() == IdentifierToken || $next->value() =~ /$nextDictionaryMember_1/) {
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
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextDictionaryMember_1/) {
        my $member = IDLDictionaryMember->new();

        if ($next->value eq "required") {
            $self->assertTokenValue($self->getToken(), "required", __LINE__);
            $member->isRequired(1);

            my $type = $self->parseTypeWithExtendedAttributes();
            $member->type($type);
        } else {
            $member->isRequired(0);

            my $type = $self->parseType();
            $self->moveExtendedAttributesApplicableToTypes($type, $extendedAttributeList);
            
            $member->type($type);
        }

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "dictionary-member");
        $member->extendedAttributes($extendedAttributeList);

        my $nameToken = $self->getToken();
        $self->assertTokenType($nameToken, IdentifierToken);
        $member->name($nameToken->value);
        $member->default($self->parseDefault());
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return $member;
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

sub parseInheritance
{
    my $self = shift;

    my $next = $self->nextToken();
    if ($next->value() eq ":") {
        $self->assertTokenValue($self->getToken(), ":", __LINE__);
        return $self->parseName();
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseEnum
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "enum") {
        my $enum = IDLEnum->new();
        $self->assertTokenValue($self->getToken(), "enum", __LINE__);
        my $enumNameToken = $self->getToken();
        $self->assertTokenType($enumNameToken, IdentifierToken);
        my $name = identifierRemoveNullablePrefix($enumNameToken->value());
        $enum->name($name);
        $enum->type(makeSimpleType($name));
        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        push(@{$enum->values}, @{$self->parseEnumValueList()});
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        
        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "enum");
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
        my $callback = IDLCallbackFunction->new();

        my $nameToken = $self->getToken();
        $self->assertTokenType($nameToken, IdentifierToken);

        $callback->type(makeSimpleType($nameToken->value()));

        $self->assertTokenValue($self->getToken(), "=", __LINE__);

        my $operation = IDLOperation->new();
        $operation->type($self->parseType());
        
        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "callback-function", "operation");
        $operation->extendedAttributes($extendedAttributeList);

        $self->assertTokenValue($self->getToken(), "(", __LINE__);

        push(@{$operation->arguments}, @{$self->parseArgumentList()});

        $self->assertTokenValue($self->getToken(), ")", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);

        $callback->operation($operation);
        $callback->extendedAttributes($extendedAttributeList);

        return $callback;
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
        my $typedef = IDLTypedef->new();

        my $type = $self->parseTypeWithExtendedAttributes();
        $typedef->type($type);

        my $nameToken = $self->getToken();
        $self->assertTokenType($nameToken, IdentifierToken);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        my $name = $nameToken->value();
        die "typedef redefinition for " . $name . " at " . $self->{Line} if (exists $typedefs{$name} && $typedef->type->name ne $typedefs{$name}->type->name);
        $typedefs{$name} = $typedef;
        return;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseIncludesStatement
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken) {
        my $includesStatement = IDLIncludesStatement->new();
    
        my $interfaceIdentifier = $self->parseName();
        $includesStatement->interfaceIdentifier($interfaceIdentifier);

        $self->assertTokenValue($self->getToken(), "includes", __LINE__);

        my $mixinIdentifier = $self->parseName();
        $includesStatement->mixinIdentifier($mixinIdentifier);

        $self->assertTokenValue($self->getToken(), ";", __LINE__);

        return $includesStatement;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseConst
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "const") {
        my $newDataNode = IDLConstant->new();
        $self->assertTokenValue($self->getToken(), "const", __LINE__);
        my $type = $self->parseConstType();
        $newDataNode->type($type);
        my $constNameToken = $self->getToken();
        $self->assertTokenType($constNameToken, IdentifierToken);
        $newDataNode->name(identifierRemoveNullablePrefix($constNameToken->value()));
        $self->assertTokenValue($self->getToken(), "=", __LINE__);
        $newDataNode->value($self->parseConstValue());
        $self->assertTokenValue($self->getToken(), ";", __LINE__);

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "constant");
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

sub parseReadOnlyMember
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "readonly") {
        $self->assertTokenValue($self->getToken(), "readonly", __LINE__);

        my $next = $self->nextToken();
        if ($next->value() eq "attribute") {
            my $attribute = $self->parseAttributeRest($extendedAttributeList);
            $attribute->isReadOnly(1);
            return $attribute;
        }
        if ($next->value() eq "maplike") {
            return $self->parseMapLikeRest($extendedAttributeList, 1);
        }
        if ($next->value() eq "setlike") {
            return $self->parseSetLikeRest($extendedAttributeList, 1);
        }
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseConstructor
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "constructor") {
        $self->assertTokenValue($self->getToken(), "constructor", __LINE__);

        my $operation = IDLOperation->new();
        $operation->isConstructor(1);

        $self->assertTokenValue($self->getToken(), "(", __LINE__);

        push(@{$operation->arguments}, @{$self->parseArgumentList()});

        $self->assertTokenValue($self->getToken(), ")", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "operation"); #FIXME: This should be "constructor"
        $operation->extendedAttributes($extendedAttributeList);

        return $operation;
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
        my $next = $self->nextToken();
        my $newDataNode;
        if ($next->value() ne ";") {
            $newDataNode = $self->parseSerializerRest($extendedAttributeList);
            my $next = $self->nextToken();
        } else {
            $newDataNode = IDLSerializable->new();
        }

        my $toJSONOperation = IDLOperation->new();
        $toJSONOperation->name("toJSON");

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "operation");
        $toJSONOperation->extendedAttributes($extendedAttributeList);
        $toJSONOperation->isSerializer(1);
        push(@{$newDataNode->operations}, $toJSONOperation);

        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return $newDataNode;
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

        return $self->parseSerializationPattern();

    }
    if ($next->type() == IdentifierToken || $next->value() eq "(") {
        return $self->parseOperationRest($extendedAttributeList);
    }
}

sub parseSerializationPattern
{
    my $self = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "{") {
        $self->assertTokenValue($self->getToken(), "{", __LINE__);
        my $newDataNode = IDLSerializable->new();
        $self->parseSerializationAttributes($newDataNode);
        $self->assertTokenValue($self->getToken(), "}", __LINE__);
        return $newDataNode;
    }
    if ($next->value() eq "[") {
        die "Serialization of lists pattern is not currently supported.";
    }
    if ($next->type() == IdentifierToken) {
        my @attributes = ();
        my $token = $self->getToken();
        $self->assertTokenType($token, IdentifierToken);
        push(@attributes, $token->value());

        my $newDataNode = IDLSerializable->new();
        $newDataNode->attributes(\@attributes);

        return $newDataNode;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseSerializationAttributes
{
    my $self = shift;
    my $serializable = shift;

    my @attributes = ();
    my @identifiers = $self->parseIdentifierList();

    for my $identifier (@identifiers) {
        if ($identifier eq "getter") {
            $serializable->hasGetter(1);
            die "Serializer getter keyword is not currently supported.";
        }

        if ($identifier eq "inherit") {
            $serializable->hasInherit(1);
            next;
        }

        if ($identifier eq "attribute") {
            $serializable->hasAttribute(1);
            # Attributes will be filled in via applyMemberList()
            next;
        }

        push(@attributes, $identifier);
    }

    $serializable->attributes(\@attributes);
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

sub parseStringifier
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "stringifier") {
        $self->assertTokenValue($self->getToken(), "stringifier", __LINE__);

        $next = $self->nextToken();
        if ($next->value() eq ";") {
            $self->assertTokenValue($self->getToken(), ";", __LINE__);

            my $operation = IDLOperation->new();
            $operation->isStringifier(1);
            $operation->name("");
            $operation->type(makeSimpleType("DOMString"));
            $operation->extendedAttributes($extendedAttributeList);

            return $operation;
        } else {
            my $attributeOrOperation = $self->parseAttributeOrOperationForStringifierOrStatic($extendedAttributeList);
            $attributeOrOperation->isStringifier(1);

            return $attributeOrOperation;
        }
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseStaticMember
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "static") {
        $self->assertTokenValue($self->getToken(), "static", __LINE__);

        my $attributeOrOperation = $self->parseAttributeOrOperationForStringifierOrStatic($extendedAttributeList);
        $attributeOrOperation->isStatic(1);

        return $attributeOrOperation;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseAttributeOrOperationForStringifierOrStatic
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() =~ /$nextOptionallyReadonlyAttribute_1/) {
        my $isReadOnly = $self->parseReadOnly();
        my $attribute = $self->parseAttributeRest($extendedAttributeList);
        $attribute->isReadOnly($isReadOnly);
        return $attribute;
    }

    if ($next->type() == IdentifierToken || $next->value() =~ /$nextRegularOperation_1/) {
        return $self->parseRegularOperation($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseInheritAttribute
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "inherit") {
        $self->assertTokenValue($self->getToken(), "inherit", __LINE__);

        my $attribute = $self->parseAttributeRest($extendedAttributeList);
        $attribute->isInherit(1);

        return $attribute;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseAttributeRest
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "attribute") {
        $self->assertTokenValue($self->getToken(), "attribute", __LINE__);

        my $attribute = IDLAttribute->new();
        
        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "attribute");
        $attribute->extendedAttributes($extendedAttributeList);

        my $type = $self->parseTypeWithExtendedAttributes();
        $attribute->type($type);

        my $token = $self->getToken();
        $self->assertTokenType($token, IdentifierToken);
        $attribute->name(identifierRemoveNullablePrefix($token->value()));
        $self->assertTokenValue($self->getToken(), ";", __LINE__);

        return $attribute;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
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

sub parseOperation
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() =~ /$nextSpecials_1/) {
        return $self->parseSpecialOperation($extendedAttributeList);
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextRegularOperation_1/) {
        return $self->parseRegularOperation($extendedAttributeList);
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseRegularOperation
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextRegularOperation_1/) {
        my $type = $self->parseType();

        # NOTE: This is a non-standard addition. In WebIDL, there is no way to associate
        # extended attributes with a return type.
        $self->moveExtendedAttributesApplicableToTypes($type, $extendedAttributeList);

        my $operation = $self->parseOperationRest($extendedAttributeList);
        $operation->type($type);

        return $operation;
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
        my $returnType = $self->parseType();

        # NOTE: This is a non-standard addition. In WebIDL, there is no way to associate
        # extended attributes with a return type.
        $self->moveExtendedAttributesApplicableToTypes($returnType, $extendedAttributeList);

        my $operation = $self->parseOperationRest($extendedAttributeList);
        $operation->type($returnType);
        $operation->specials(\@specials);

        return $operation;
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
    if ($next->value() eq "deleter") {
        $self->assertTokenValue($self->getToken(), "deleter", __LINE__);
        return "deleter";
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseAsyncIterable
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "async") {
        $self->assertTokenValue($self->getToken(), "async", __LINE__);
        $self->assertTokenValue($self->getToken(), "iterable", __LINE__);

        my $asyncIterable = IDLAsyncIterable->new();

        $self->assertTokenValue($self->getToken(), "<", __LINE__);
        my $type1 = $self->parseTypeWithExtendedAttributes();

        if ($self->nextToken()->value() eq ",") {
            $self->assertTokenValue($self->getToken(), ",", __LINE__);

            my $type2 = $self->parseTypeWithExtendedAttributes();
            $asyncIterable->isKeyValue(1);
            $asyncIterable->keyType($type1);
            $asyncIterable->valueType($type2);
        } else {
            $asyncIterable->isKeyValue(0);
            $asyncIterable->valueType($type1);
        }
        $self->assertTokenValue($self->getToken(), ">", __LINE__);

        if ($self->nextToken()->value() eq "(") {
            $self->assertTokenValue($self->getToken(), "(", __LINE__);
            push(@{$asyncIterable->arguments}, @{$self->parseArgumentList()});
            $self->assertTokenValue($self->getToken(), ")", __LINE__);
        }

        $self->assertTokenValue($self->getToken(), ";", __LINE__);

        die "'async iterable` interfaces are not currently supported by the code generators.";

        return $asyncIterable;
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

    my $newDataNode = IDLIterable->new();

    $self->assertExtendedAttributesValidForContext($extendedAttributeList, "iterable");
    $newDataNode->extendedAttributes($extendedAttributeList);

    $self->assertTokenValue($self->getToken(), "<", __LINE__);
    my $type1 = $self->parseTypeWithExtendedAttributes();

    if ($self->nextToken()->value() eq ",") {
        $self->assertTokenValue($self->getToken(), ",", __LINE__);

        my $type2 = $self->parseTypeWithExtendedAttributes();
        $newDataNode->isKeyValue(1);
        $newDataNode->keyType($type1);
        $newDataNode->valueType($type2);
    } else {
        $newDataNode->isKeyValue(0);
        $newDataNode->valueType($type1);
    }
    $self->assertTokenValue($self->getToken(), ">", __LINE__);

    # FIXME: Synthetic operations should not be added during parsing. Instead, the CodeGenerator
    # should be responsible for them.

    my $symbolIteratorOperation = IDLOperation->new();
    $symbolIteratorOperation->name("[Symbol.Iterator]");
    $symbolIteratorOperation->extendedAttributes($extendedAttributeList);
    $symbolIteratorOperation->isIterable(1);

    my $entriesOperation = IDLOperation->new();
    $entriesOperation->name("entries");
    $entriesOperation->extendedAttributes($extendedAttributeList);
    $entriesOperation->isIterable(1);

    my $keysOperation = IDLOperation->new();
    $keysOperation->name("keys");
    $keysOperation->extendedAttributes($extendedAttributeList);
    $keysOperation->isIterable(1);

    my $valuesOperation = IDLOperation->new();
    $valuesOperation->name("values");
    $valuesOperation->extendedAttributes($extendedAttributeList);
    $valuesOperation->isIterable(1);

    my $forEachOperation = IDLOperation->new();
    $forEachOperation->name("forEach");
    $forEachOperation->extendedAttributes($extendedAttributeList);
    $forEachOperation->isIterable(1);
    my $forEachArgument = IDLArgument->new();
    $forEachArgument->name("callback");
    $forEachArgument->type(makeSimpleType("any"));
    push(@{$forEachOperation->arguments}, ($forEachArgument));

    push(@{$newDataNode->operations}, $symbolIteratorOperation);
    push(@{$newDataNode->operations}, $entriesOperation);
    push(@{$newDataNode->operations}, $keysOperation);
    push(@{$newDataNode->operations}, $valuesOperation);
    push(@{$newDataNode->operations}, $forEachOperation);

    return $newDataNode;
}

sub parseMapLikeRest
{
    my $self = shift;
    my $extendedAttributeList = shift;
    my $isReadOnly = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "maplike") {
        $self->assertTokenValue($self->getToken(), "maplike", __LINE__);
        my $mapLikeNode = $self->parseMapLikeProperties($extendedAttributeList, $isReadOnly);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return $mapLikeNode;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseMapLikeProperties
{
    my $self = shift;
    my $extendedAttributeList = shift;
    my $isReadOnly = shift;

    my $maplike = IDLMapLike->new();
    $maplike->extendedAttributes($extendedAttributeList);
    $maplike->isReadOnly($isReadOnly);

    $self->assertTokenValue($self->getToken(), "<", __LINE__);
    $maplike->keyType($self->parseTypeWithExtendedAttributes());
    $self->assertTokenValue($self->getToken(), ",", __LINE__);
    $maplike->valueType($self->parseTypeWithExtendedAttributes());
    $self->assertTokenValue($self->getToken(), ">", __LINE__);

    # FIXME: Synthetic operations should not be added during parsing. Instead, the CodeGenerator
    # should be responsible for them.

    my $notEnumerableExtendedAttributeList = $extendedAttributeList;
    $notEnumerableExtendedAttributeList->{NotEnumerable} = 1;

    my $sizeAttribute = IDLAttribute->new();
    $sizeAttribute->name("size");
    $sizeAttribute->isMapLike(1);
    $sizeAttribute->extendedAttributes($extendedAttributeList);
    $sizeAttribute->isReadOnly(1);
    $sizeAttribute->type(makeSimpleType("any"));
    push(@{$maplike->attributes}, $sizeAttribute);

    my $getOperation = IDLOperation->new();
    $getOperation->name("get");
    $getOperation->isMapLike(1);
    my $getArgument = IDLArgument->new();
    $getArgument->name("key");
    $getArgument->type($maplike->keyType);
    $getArgument->extendedAttributes($extendedAttributeList);
    push(@{$getOperation->arguments}, ($getArgument));
    $getOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $getOperation->type(makeSimpleType("any"));

    my $hasOperation = IDLOperation->new();
    $hasOperation->name("has");
    $hasOperation->isMapLike(1);
    my $hasArgument = IDLArgument->new();
    $hasArgument->name("key");
    $hasArgument->type($maplike->keyType);
    $hasArgument->extendedAttributes($extendedAttributeList);
    push(@{$hasOperation->arguments}, ($hasArgument));
    $hasOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $hasOperation->type(makeSimpleType("any"));

    my $entriesOperation = IDLOperation->new();
    $entriesOperation->name("entries");
    $entriesOperation->isMapLike(1);
    $entriesOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $entriesOperation->type(makeSimpleType("any"));

    my $keysOperation = IDLOperation->new();
    $keysOperation->name("keys");
    $keysOperation->isMapLike(1);
    $keysOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $keysOperation->type(makeSimpleType("any"));

    my $valuesOperation = IDLOperation->new();
    $valuesOperation->name("values");
    $valuesOperation->isMapLike(1);
    $valuesOperation->extendedAttributes($extendedAttributeList);
    $valuesOperation->extendedAttributes->{NotEnumerable} = 1;
    $valuesOperation->type(makeSimpleType("any"));

    my $forEachOperation = IDLOperation->new();
    $forEachOperation->name("forEach");
    $forEachOperation->isMapLike(1);
    $forEachOperation->extendedAttributes({});
    $forEachOperation->type(makeSimpleType("any"));
    my $forEachArgument = IDLArgument->new();
    $forEachArgument->name("callback");
    $forEachArgument->type(makeSimpleType("any"));
    $forEachArgument->extendedAttributes($extendedAttributeList);
    push(@{$forEachOperation->arguments}, ($forEachArgument));

    push(@{$maplike->operations}, $getOperation);
    push(@{$maplike->operations}, $hasOperation);
    push(@{$maplike->operations}, $entriesOperation);
    push(@{$maplike->operations}, $keysOperation);
    push(@{$maplike->operations}, $valuesOperation);
    push(@{$maplike->operations}, $forEachOperation);

    return $maplike if $isReadOnly;

    my $setOperation = IDLOperation->new();
    $setOperation->name("set");
    $setOperation->isMapLike(1);
    my $setKeyArgument = IDLArgument->new();
    $setKeyArgument->name("key");
    $setKeyArgument->type($maplike->keyType);
    $setKeyArgument->extendedAttributes($extendedAttributeList);
    my $setValueArgument = IDLArgument->new();
    $setValueArgument->name("value");
    $setValueArgument->type($maplike->valueType);
    $setValueArgument->extendedAttributes($extendedAttributeList);
    push(@{$setOperation->arguments}, ($setKeyArgument));
    push(@{$setOperation->arguments}, ($setValueArgument));
    $setOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $setOperation->type(makeSimpleType("any"));

    my $clearOperation = IDLOperation->new();
    $clearOperation->name("clear");
    $clearOperation->isMapLike(1);
    $clearOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $clearOperation->type(makeSimpleType("undefined"));

    my $deleteOperation = IDLOperation->new();
    $deleteOperation->name("delete");
    $deleteOperation->isMapLike(1);
    my $deleteArgument = IDLArgument->new();
    $deleteArgument->name("key");
    $deleteArgument->type($maplike->keyType);
    $deleteArgument->extendedAttributes($extendedAttributeList);
    push(@{$deleteOperation->arguments}, ($deleteArgument));
    $deleteOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $deleteOperation->type(makeSimpleType("any"));

    push(@{$maplike->operations}, $setOperation);
    push(@{$maplike->operations}, $clearOperation);
    push(@{$maplike->operations}, $deleteOperation);

    return $maplike;
}

sub parseSetLikeRest
{
    my $self = shift;
    my $extendedAttributeList = shift;
    my $isReadOnly = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "setlike") {
        $self->assertTokenValue($self->getToken(), "setlike", __LINE__);
        my $setLikeNode = $self->parseSetLikeProperties($extendedAttributeList, $isReadOnly);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);
        return $setLikeNode;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseSetLikeProperties
{
    my $self = shift;
    my $extendedAttributeList = shift;
    my $isReadOnly = shift;

    my $setlike = IDLSetLike->new();
    $setlike->extendedAttributes($extendedAttributeList);

    $self->assertTokenValue($self->getToken(), "<", __LINE__);
    $setlike->itemType($self->parseTypeWithExtendedAttributes());
    $self->assertTokenValue($self->getToken(), ">", __LINE__);

    # FIXME: Synthetic operations should not be added during parsing. Instead, the CodeGenerator
    # should be responsible for them.

    my $notEnumerableExtendedAttributeList = $extendedAttributeList;
    $notEnumerableExtendedAttributeList->{NotEnumerable} = 1;

    my $sizeAttribute = IDLAttribute->new();
    $sizeAttribute->name("size");
    $sizeAttribute->isSetLike(1);
    $sizeAttribute->extendedAttributes($extendedAttributeList);
    $sizeAttribute->isReadOnly(1);
    $sizeAttribute->type(makeSimpleType("any"));
    push(@{$setlike->attributes}, $sizeAttribute);

    my $hasOperation = IDLOperation->new();
    $hasOperation->name("has");
    $hasOperation->isSetLike(1);
    my $hasArgument = IDLArgument->new();
    $hasArgument->name("key");
    $hasArgument->type($setlike->itemType);
    $hasArgument->extendedAttributes($extendedAttributeList);
    push(@{$hasOperation->arguments}, ($hasArgument));
    $hasOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $hasOperation->type(makeSimpleType("any"));

    my $entriesOperation = IDLOperation->new();
    $entriesOperation->name("entries");
    $entriesOperation->isSetLike(1);
    $entriesOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $entriesOperation->type(makeSimpleType("any"));

    my $keysOperation = IDLOperation->new();
    $keysOperation->name("keys");
    $keysOperation->isSetLike(1);
    $keysOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $keysOperation->type(makeSimpleType("any"));

    my $valuesOperation = IDLOperation->new();
    $valuesOperation->name("values");
    $valuesOperation->isSetLike(1);
    $valuesOperation->extendedAttributes($extendedAttributeList);
    $valuesOperation->extendedAttributes->{NotEnumerable} = 1;
    $valuesOperation->type(makeSimpleType("any"));

    my $forEachOperation = IDLOperation->new();
    $forEachOperation->name("forEach");
    $forEachOperation->isSetLike(1);
    my $forEachArgument = IDLArgument->new();
    $forEachArgument->name("callback");
    $forEachArgument->type(makeSimpleType("any"));
    $forEachArgument->extendedAttributes($extendedAttributeList);
    push(@{$forEachOperation->arguments}, ($forEachArgument));
    $forEachOperation->extendedAttributes($extendedAttributeList);
    $forEachOperation->extendedAttributes->{Enumerable} = 1;
    $forEachOperation->type(makeSimpleType("any"));

    push(@{$setlike->operations}, $hasOperation);
    push(@{$setlike->operations}, $entriesOperation);
    push(@{$setlike->operations}, $keysOperation);
    push(@{$setlike->operations}, $valuesOperation);
    push(@{$setlike->operations}, $forEachOperation);

    return $setlike if $isReadOnly;

    my $addOperation = IDLOperation->new();
    $addOperation->name("add");
    $addOperation->isSetLike(1);
    my $addArgument = IDLArgument->new();
    $addArgument->name("key");
    $addArgument->type($setlike->itemType);
    $addArgument->extendedAttributes($extendedAttributeList);
    push(@{$addOperation->arguments}, ($addArgument));
    $addOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $addOperation->type(makeSimpleType("any"));

    my $clearOperation = IDLOperation->new();
    $clearOperation->name("clear");
    $clearOperation->isSetLike(1);
    $clearOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $clearOperation->type(makeSimpleType("undefined"));

    my $deleteOperation = IDLOperation->new();
    $deleteOperation->name("delete");
    $deleteOperation->isSetLike(1);
    my $deleteArgument = IDLArgument->new();
    $deleteArgument->name("key");
    $deleteArgument->type($setlike->itemType);
    $deleteArgument->extendedAttributes($extendedAttributeList);
    push(@{$deleteOperation->arguments}, ($deleteArgument));
    $deleteOperation->extendedAttributes($notEnumerableExtendedAttributeList);
    $deleteOperation->type(makeSimpleType("any"));

    push(@{$setlike->operations}, $addOperation);
    push(@{$setlike->operations}, $clearOperation);
    push(@{$setlike->operations}, $deleteOperation);

    return $setlike;
}

sub parseOperationRest
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->type() == IdentifierToken || $next->value() eq "(") {
        my $operation = IDLOperation->new();

        my $name = $self->parseOptionalIdentifier();
        $operation->name(identifierRemoveNullablePrefix($name));

        $self->assertTokenValue($self->getToken(), "(", $name, __LINE__);

        push(@{$operation->arguments}, @{$self->parseArgumentList()});

        $self->assertTokenValue($self->getToken(), ")", __LINE__);
        $self->assertTokenValue($self->getToken(), ";", __LINE__);

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "operation");
        $operation->extendedAttributes($extendedAttributeList);

        return $operation;
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
        my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
        my $argument = $self->parseArgumentsRest($extendedAttributeList);
        return $argument;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseArgumentsRest
{
    my $self = shift;
    my $extendedAttributeList = shift;

    my $next = $self->nextToken();
    if ($next->value() eq "optional") {
        $self->assertTokenValue($self->getToken(), "optional", __LINE__);

        my $argument = IDLArgument->new();

        my $type = $self->parseTypeWithExtendedAttributes();
        $argument->type($type);
        $argument->isOptional(1);
        $argument->name($self->parseArgumentName());
        $argument->default($self->parseDefault());

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "argument");
        $argument->extendedAttributes($extendedAttributeList);

        return $argument;
    } else {
        my $argument = IDLArgument->new();
    
        my $type = $self->parseType();
        $self->moveExtendedAttributesApplicableToTypes($type, $extendedAttributeList);

        $argument->type($type);
        $argument->isOptional(0);
        $argument->isVariadic($self->parseEllipsis());
        $argument->name($self->parseArgumentName());

        $self->assertExtendedAttributesValidForContext($extendedAttributeList, "argument");
        $argument->extendedAttributes($extendedAttributeList);

        return $argument;
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

sub parseExtendedAttributeListAllowEmpty
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "[") {
        return $self->parseExtendedAttributeList();
    }
    return {};
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

    $attrs->{$name} = "VALUE_IS_MISSING";
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
        return \@arguments;
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
    if ($next->value() eq "async") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "attribute") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "callback") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "const") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "constructor") {
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
    if ($next->value() eq "getter") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "includes") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "inherit") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "interface") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "iterable") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "maplike") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "mixin") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "namespace") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "partial") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "readonly") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "required") {
        return $self->getToken()->value();
    }
    if ($next->value() eq "setlike") {
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

    my $extendedAttributeList = {};

    if ($next->value() eq "(") {
        my $unionType = $self->parseUnionType();
        $unionType->isNullable($self->parseNull());
        $unionType->extendedAttributes($extendedAttributeList);
        return $unionType;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextType_1/) {
        my $singleType = $self->parseSingleType();
        $singleType->extendedAttributes($extendedAttributeList);
        return $singleType;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseTypeWithExtendedAttributes
{
    my $self = shift;
    
    my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
    $self->assertExtendedAttributesValidForContext($extendedAttributeList, "type");

    my $next = $self->nextToken();
    if ($next->value() eq "(") {
        my $unionType = $self->parseUnionType();
        $unionType->isNullable($self->parseNull());
        $unionType->extendedAttributes($extendedAttributeList);
        return $unionType;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextType_1/) {
        my $singleType = $self->parseSingleType();
        $singleType->extendedAttributes($extendedAttributeList);
        return $singleType;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseSingleType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "any") {
        $self->assertTokenValue($self->getToken(), "any", __LINE__);
        
        my $anyType = IDLType->new();
        $anyType->name("any");
        return $anyType;
    }
    if ($next->value() eq "Promise") {
        $self->assertTokenValue($self->getToken(), "Promise", __LINE__);
        $self->assertTokenValue($self->getToken(), "<", __LINE__);

        my $subtype = $self->parseType();

        $self->assertTokenValue($self->getToken(), ">", __LINE__);

        my $type = IDLType->new();
        $type->name("Promise");
        push(@{$type->subtypes}, $subtype);
        return $type;
    }
    if ($next->type() == IdentifierToken || $next->value() =~ /$nextSingleType_1/) {
        my $distinguishableType = $self->parseDistinguishableType();
        $distinguishableType->isNullable($self->parseNull());
        return $distinguishableType;
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parseUnionType
{
    my $self = shift;
    my $next = $self->nextToken();

    my $unionType = IDLType->new();
    $unionType->name("UNION");
    $unionType->isUnion(1);

    if ($next->value() eq "(") {
        $self->assertTokenValue($self->getToken(), "(", __LINE__);
        
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

    my $extendedAttributeList = $self->parseExtendedAttributeListAllowEmpty();
    $self->assertExtendedAttributesValidForContext($extendedAttributeList, "type");

    my $next = $self->nextToken();

    if ($next->value() eq "(") {
        my $unionType = $self->parseUnionType();
        $unionType->isNullable($self->parseNull());
        $unionType->extendedAttributes($extendedAttributeList);
        return $unionType;
    }

    if ($next->type() == IdentifierToken || $next->value() =~ /$nextSingleType_1/) {
        my $distinguishableType = $self->parseDistinguishableType();
        $distinguishableType->isNullable($self->parseNull());
        $distinguishableType->extendedAttributes($extendedAttributeList);
        return $distinguishableType;
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

sub parseDistinguishableType
{
    my $self = shift;
    my $next = $self->nextToken();

    my $type = IDLType->new();

    if ($next->value() =~ /$nextPrimitiveType_1/) {
        $type->name($self->parsePrimitiveType());
        return $type;
    }
    if ($next->value() =~ /$nextStringType_1/) {
        $type->name($self->parseStringType());
        return $type;
    }
    if ($next->value() eq "sequence") {
        $self->assertTokenValue($self->getToken(), "sequence", __LINE__);
        $self->assertTokenValue($self->getToken(), "<", __LINE__);

        my $subtype = $self->parseTypeWithExtendedAttributes();

        $self->assertTokenValue($self->getToken(), ">", __LINE__);

        $type->name("sequence");
        push(@{$type->subtypes}, $subtype);

        return $type;
    }
    if ($next->value() eq "object") {
        $self->assertTokenValue($self->getToken(), "object", __LINE__);

        $type->name("object");
        return $type;
    }
    if ($next->value() eq "symbol") {
        $self->assertTokenValue($self->getToken(), "symbol", __LINE__);

        $type->name("symbol");
        return $type;
    }
    if ($next->value() eq "FrozenArray") {
        $self->assertTokenValue($self->getToken(), "FrozenArray", __LINE__);
        $self->assertTokenValue($self->getToken(), "<", __LINE__);

        my $subtype = $self->parseTypeWithExtendedAttributes();

        $self->assertTokenValue($self->getToken(), ">", __LINE__);

        $type->name("FrozenArray");
        push(@{$type->subtypes}, $subtype);

        return $type;
    }
    if ($next->value() eq "ObservableArray") {
        $self->assertTokenValue($self->getToken(), "ObservableArray", __LINE__);
        $self->assertTokenValue($self->getToken(), "<", __LINE__);

        my $subtype = $self->parseTypeWithExtendedAttributes();

        $self->assertTokenValue($self->getToken(), ">", __LINE__);

        $type->name("ObservableArray");
        push(@{$type->subtypes}, $subtype);

        die "The ObservableArray type is not currently supported by the code generators.";

        return $type;
    }
    if ($next->value() eq "record") {
        $self->assertTokenValue($self->getToken(), "record", __LINE__);
        $self->assertTokenValue($self->getToken(), "<", __LINE__);

        my $keyType = IDLType->new();
        $keyType->name($self->parseStringType());

        $self->assertTokenValue($self->getToken(), ",", __LINE__);

        my $valueType = $self->parseTypeWithExtendedAttributes();

        $self->assertTokenValue($self->getToken(), ">", __LINE__);

        $type->name("record");
        push(@{$type->subtypes}, $keyType);
        push(@{$type->subtypes}, $valueType);

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

    my $type = IDLType->new();

    if ($next->value() =~ /$nextPrimitiveType_1/) {
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

sub parseStringType
{
    my $self = shift;
    my $next = $self->nextToken();
    if ($next->value() eq "ByteString") {
        $self->assertTokenValue($self->getToken(), "ByteString", __LINE__);
        return "ByteString";
    }
    if ($next->value() eq "DOMString") {
        $self->assertTokenValue($self->getToken(), "DOMString", __LINE__);
        return "DOMString";
    }
    if ($next->value() eq "USVString") {
        $self->assertTokenValue($self->getToken(), "USVString", __LINE__);
        return "USVString";
    }
    $self->assertUnexpectedToken($next->value(), __LINE__);
}

sub parsePrimitiveType
{
    my $self = shift;

    my $next = $self->nextToken();
    if ($next->value() =~ /$nextIntegerType_1/) {
        return $self->parseUnsignedIntegerType();
    }
    if ($next->value() =~ /$nextFloatType_1/) {
        return $self->parseUnrestrictedFloatType();
    }
    if ($next->value() eq "undefined") {
        $self->assertTokenValue($self->getToken(), "undefined", __LINE__);
        return "undefined";
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
        if (ref($item) eq "IDLAttribute") {
            push(@{$interface->attributes}, $item);
            next;
        }
        if (ref($item) eq "IDLConstant") {
            push(@{$interface->constants}, $item);
            next;
        }
        if (ref($item) eq "IDLIterable") {
            $interface->iterable($item);
            next;
        }
        if (ref($item) eq "IDLAsyncIterable") {
            $interface->asyncIterable($item);
            next;
        }
        if (ref($item) eq "IDLMapLike") {
            $interface->mapLike($item);
            next;
        }
        if (ref($item) eq "IDLSetLike") {
            $interface->setLike($item);
            next;
        }
        if (ref($item) eq "IDLOperation") {
            if ($item->isConstructor) {
                push(@{$interface->constructors}, $item);
            } elsif ($item->name eq "") {
                push(@{$interface->anonymousOperations}, $item);
            } else {
                push(@{$interface->operations}, $item);
            }
            next;
        }
        if (ref($item) eq "IDLSerializable") {
            $interface->serializable($item);
            next;
        }
    }

    if ($interface->serializable) {
        my $numSerializerAttributes = @{$interface->serializable->attributes};
        if ($interface->serializable->hasAttribute) {
            foreach my $attribute (@{$interface->attributes}) {
                push(@{$interface->serializable->attributes}, $attribute->name);
            }
        } elsif ($numSerializerAttributes == 0) {
            foreach my $attribute (@{$interface->attributes}) {
                push(@{$interface->serializable->attributes}, $attribute->name);
            }
        }
    }
}

sub applyExtendedAttributeList
{
    my $interface = shift;
    my $extendedAttributeList = shift;

    if (defined $extendedAttributeList->{"LegacyFactoryFunction"}) {
        my $newDataNode = IDLOperation->new();
        $newDataNode->isConstructor(1);
        $newDataNode->name("LegacyFactoryFunction");
        $newDataNode->extendedAttributes($extendedAttributeList);
        my %attributes = %{$extendedAttributeList->{"LegacyFactoryFunction"}};
        my @attributeKeys = keys (%attributes);
        my $constructorName = $attributeKeys[0];
        push(@{$newDataNode->arguments}, @{$attributes{$constructorName}});
        $extendedAttributeList->{"LegacyFactoryFunction"} = $constructorName;
        push(@{$interface->constructors}, $newDataNode);
    }
    
    $interface->extendedAttributes($extendedAttributeList);
}

1;

