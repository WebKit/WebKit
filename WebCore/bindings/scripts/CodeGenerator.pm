#
# WebKit IDL parser
# 
# Copyright (C) 2005 Nikolas Zimmermann <wildfox@kde.org>
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
# Copyright (C) 2007 Apple Inc. All rights reserved.
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
# aint with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.
# 

package CodeGenerator;

my $useDocument = "";
my $useGenerator = "";
my $useOutputDir = "";
my $useDirectories = "";
my $useLayerOnTop = 0;
my $preprocessor;

my $codeGenerator = 0;

my $verbose = 0;

my %primitiveTypeHash = ("int" => 1, "short" => 1, "long" => 1, "long long" => 1, 
                         "unsigned int" => 1, "unsigned short" => 1,
                         "unsigned long" => 1, "unsigned long long" => 1, 
                         "float" => 1, "double" => 1, 
                         "boolean" => 1, "void" => 1);

my %podTypeHash = ("RGBColor" => 1, "SVGNumber" => 1, "SVGTransform" => 1);
my %podTypesWithWritablePropertiesHash = ("SVGLength" => 1, "SVGMatrix" => 1, "SVGPoint" => 1, "SVGRect" => 1);
my %stringTypeHash = ("DOMString" => 1, "AtomicString" => 1);

my %nonPointerTypeHash = ("DOMTimeStamp" => 1, "CompareHow" => 1, "SVGPaintType" => 1);

my %svgAnimatedTypeHash = ("SVGAnimatedAngle" => 1, "SVGAnimatedBoolean" => 1,
                           "SVGAnimatedEnumeration" => 1, "SVGAnimatedInteger" => 1,
                           "SVGAnimatedLength" => 1, "SVGAnimatedLengthList" => 1,
                           "SVGAnimatedNumber" => 1, "SVGAnimatedNumberList" => 1,
                           "SVGAnimatedPreserveAspectRatio" => 1,
                           "SVGAnimatedRect" => 1, "SVGAnimatedString" => 1,
                           "SVGAnimatedTransformList" => 1);

# Helpers for 'ScanDirectory'
my $endCondition = 0;
my $foundFilename = "";
my @foundFilenames = ();
my $ignoreParent = 1;
my $defines = "";

# Default constructor
sub new
{
    my $object = shift;
    my $reference = { };

    $useDirectories = shift;
    $useGenerator = shift;
    $useOutputDir = shift;
    $useLayerOnTop = shift;
    $preprocessor = shift;

    bless($reference, $object);
    return $reference;
}

sub StripModule($)
{
    my $object = shift;
    my $name = shift;
    $name =~ s/[a-zA-Z0-9]*:://;
    return $name;
}

sub ProcessDocument
{
    my $object = shift;
    $useDocument = shift;
    $defines = shift;

    my $ifaceName = "CodeGenerator" . $useGenerator;

    # Dynamically load external code generation perl module
    require $ifaceName . ".pm";
    $codeGenerator = $ifaceName->new($object, $useOutputDir, $useLayerOnTop, $preprocessor);
    unless (defined($codeGenerator)) {
        my $classes = $useDocument->classes;
        foreach my $class (@$classes) {
            print "Skipping $useGenerator code generation for IDL interface \"" . $class->name . "\".\n" if $verbose;
        }
        return;
    }

    # Start the actual code generation!
    $codeGenerator->GenerateModule($useDocument, $defines);

    my $classes = $useDocument->classes;
    foreach my $class (@$classes) {
        print "Generating $useGenerator bindings code for IDL interface \"" . $class->name . "\"...\n" if $verbose;
        $codeGenerator->GenerateInterface($class, $defines);
    }

    $codeGenerator->finish();
}

# Necessary for V8 bindings to determine whether an interface is descendant from Node.
# Node descendants are treated differently by DOMMap and this allows inferring the
# type statically. See more at the original change: http://codereview.chromium.org/3195.
# FIXME: Figure out a way to eliminate this JS bindings dichotomy.
sub FindParentsRecursively
{
    my $object = shift;
    my $dataNode = shift;
    my @parents = ($dataNode->name);
    foreach (@{$dataNode->parents}) {
        my $interface = $object->StripModule($_);

        $endCondition = 0;
        $foundFilename = "";
        foreach (@{$useDirectories}) {
            $object->ScanDirectory("$interface.idl", $_, $_, 0) if ($foundFilename eq "");
        }

        if ($foundFilename ne "") {
            print "  |  |>  Parsing parent IDL \"$foundFilename\" for interface \"$interface\"\n" if $verbose;

           # Step #2: Parse the found IDL file (in quiet mode).
            my $parser = IDLParser->new(1);
            my $document = $parser->Parse($foundFilename, $defines, $preprocessor, 1);

            foreach my $class (@{$document->classes}) {
                @parents = (@parents, FindParentsRecursively($object, $class));
            }
        } else {
            die("Could NOT find specified parent interface \"$interface\"!\n")
        }
    }
    return @parents;
}

sub AddMethodsConstantsAndAttributesFromParentClasses
{
    # For the passed interface, recursively parse all parent
    # IDLs in order to find out all inherited properties/methods.

    my $object = shift;
    my $dataNode = shift;

    my @parents = @{$dataNode->parents};
    my $parentsMax = @{$dataNode->parents};

    my $constantsRef = $dataNode->constants;
    my $functionsRef = $dataNode->functions;
    my $attributesRef = $dataNode->attributes;

    foreach (@{$dataNode->parents}) {
        if ($ignoreParent) {
            # Ignore first parent class, already handled by the generation itself.
            $ignoreParent = 0;
            next;
        }

        my $interface = $object->StripModule($_);

        # Step #1: Find the IDL file associated with 'interface'
        $endCondition = 0;
        $foundFilename = "";

        foreach (@{$useDirectories}) {
            $object->ScanDirectory("$interface.idl", $_, $_, 0) if ($foundFilename eq "");
        }

        if ($foundFilename ne "") {
            print "  |  |>  Parsing parent IDL \"$foundFilename\" for interface \"$interface\"\n" if $verbose;

            # Step #2: Parse the found IDL file (in quiet mode).
            my $parser = IDLParser->new(1);
            my $document = $parser->Parse($foundFilename, $defines, $preprocessor);

            foreach my $class (@{$document->classes}) {
                # Step #3: Enter recursive parent search
                AddMethodsConstantsAndAttributesFromParentClasses($object, $class);

                # Step #4: Collect constants & functions & attributes of this parent-class
                my $constantsMax = @{$class->constants};
                my $functionsMax = @{$class->functions};
                my $attributesMax = @{$class->attributes};

                print "  |  |>  -> Inheriting $constantsMax constants, $functionsMax functions, $attributesMax attributes...\n  |  |>\n" if $verbose;

                # Step #5: Concatenate data
                push(@$constantsRef, $_) foreach (@{$class->constants});
                push(@$functionsRef, $_) foreach (@{$class->functions});
                push(@$attributesRef, $_) foreach (@{$class->attributes});
            }
        } else {
            die("Could NOT find specified parent interface \"$interface\"!\n");
        }
    }
}

sub GetMethodsAndAttributesFromParentClasses
{
    # For the passed interface, recursively parse all parent
    # IDLs in order to find out all inherited properties/methods.

    my $object = shift;
    my $dataNode = shift;

    my @parents = @{$dataNode->parents};

    return if @{$dataNode->parents} == 0;

    my @parentList = ();

    foreach (@{$dataNode->parents}) {
        my $interface = $object->StripModule($_);

        # Step #1: Find the IDL file associated with 'interface'
        $endCondition = 0;
        $foundFilename = "";

        foreach (@{$useDirectories}) {
            $object->ScanDirectory("${interface}.idl", $_, $_, 0) if $foundFilename eq "";
        }

        die("Could NOT find specified parent interface \"$interface\"!\n") if $foundFilename eq "";

        print "  |  |>  Parsing parent IDL \"$foundFilename\" for interface \"$interface\"\n" if $verbose;

        # Step #2: Parse the found IDL file (in quiet mode).
        my $parser = IDLParser->new(1);
        my $document = $parser->Parse($foundFilename, $defines);

        foreach my $class (@{$document->classes}) {
            # Step #3: Enter recursive parent search
            push(@parentList, GetMethodsAndAttributesFromParentClasses($object, $class));

            # Step #4: Collect constants & functions & attributes of this parent-class

            # print "  |  |>  -> Inheriting $functionsMax functions amd $attributesMax attributes...\n  |  |>\n" if $verbose;
            my $hash = {
                "name" => $class->name,
                "functions" => $class->functions,
                "attributes" => $class->attributes
            };

            # Step #5: Concatenate data
            unshift(@parentList, $hash);
        }
    }

    return @parentList;
}

sub ParseInterface
{
    my ($object, $interfaceName) = @_;

    # Step #1: Find the IDL file associated with 'interface'
    $endCondition = 0;
    $foundFilename = "";

    foreach (@{$useDirectories}) {
        $object->ScanDirectory("${interfaceName}.idl", $_, $_, 0) if $foundFilename eq "";
    }
    die "Could NOT find specified parent interface \"$interfaceName\"!\n" if $foundFilename eq "";

    print "  |  |>  Parsing parent IDL \"$foundFilename\" for interface \"$interfaceName\"\n" if $verbose;

    # Step #2: Parse the found IDL file (in quiet mode).
    my $parser = IDLParser->new(1);
    my $document = $parser->Parse($foundFilename, $defines);

    foreach my $interface (@{$document->classes}) {
        return $interface if $interface->name eq $interfaceName;
    }

    die "Interface definition not found";
}

# Helpers for all CodeGenerator***.pm modules
sub IsPodType
{
    my $object = shift;
    my $type = shift;

    return 1 if $podTypeHash{$type};
    return 1 if $podTypesWithWritablePropertiesHash{$type};
    return 0;
}

sub IsPodTypeWithWriteableProperties
{
    my $object = shift;
    my $type = shift;

    return 1 if $podTypesWithWritablePropertiesHash{$type};
    return 0;
}

sub IsPrimitiveType
{
    my $object = shift;
    my $type = shift;

    return 1 if $primitiveTypeHash{$type};
    return 0;
}

sub IsStringType
{
    my $object = shift;
    my $type = shift;

    return 1 if $stringTypeHash{$type};
    return 0;
}

sub IsNonPointerType
{
    my $object = shift;
    my $type = shift;

    return 1 if $nonPointerTypeHash{$type} or $primitiveTypeHash{$type};
    return 0;
}

sub IsSVGAnimatedType
{
    my $object = shift;
    my $type = shift;

    return 1 if $svgAnimatedTypeHash{$type};
    return 0; 
}

# Internal Helper
sub ScanDirectory
{
    my $object = shift;

    my $interface = shift;
    my $directory = shift;
    my $useDirectory = shift;
    my $reportAllFiles = shift;

    return if ($endCondition eq 1) and ($reportAllFiles eq 0);

    my $sourceRoot = $ENV{SOURCE_ROOT};
    my $thisDir = $sourceRoot ? "$sourceRoot/$directory" : $directory;

    if (!opendir(DIR, $thisDir)) {
        opendir(DIR, $directory) or die "[ERROR] Can't open directory $thisDir or $directory: \"$!\"\n";
        $thisDir = $directory;
    }

    my @names = readdir(DIR) or die "[ERROR] Cant't read directory $thisDir \"$!\"\n";
    closedir(DIR);

    foreach my $name (@names) {
        # Skip if we already found the right file or
        # if we encounter 'exotic' stuff (ie. '.', '..', '.svn')
        next if ($endCondition eq 1) or ($name =~ /^\./);

        # Recurisvely enter directory
        if (-d "$thisDir/$name") {
            $object->ScanDirectory($interface, "$directory/$name", $useDirectory, $reportAllFiles);
            next;
        }

        # Check wheter we found the desired file
        my $condition = ($name eq $interface);
        $condition = 1 if ($interface eq "allidls") and ($name =~ /\.idl$/);

        if ($condition) {
            $foundFilename = "$thisDir/$name";

            if ($reportAllFiles eq 0) {
                $endCondition = 1;
            } else {
                push(@foundFilenames, $foundFilename);
            }
        }
    }
}

# Uppercase the first letter while respecting WebKit style guidelines. 
# E.g., xmlEncoding becomes XMLEncoding, but xmlllang becomes Xmllang.
sub WK_ucfirst
{
    my ($object, $param) = @_;
    my $ret = ucfirst($param);
    $ret =~ s/Xml/XML/ if $ret =~ /^Xml[^a-z]/;
    return $ret;
}

# Lowercase the first letter while respecting WebKit style guidelines. 
# URL becomes url, but SetURL becomes setURL.
sub WK_lcfirst
{
    my ($object, $param) = @_;
    my $ret = lcfirst($param);
    $ret =~ s/uRL/url/ if $ret =~ /^uRL/;
    $ret =~ s/jS/js/ if $ret =~ /^jS/;
    $ret =~ s/xML/xml/ if $ret =~ /^xML/;
    $ret =~ s/xSLT/xslt/ if $ret =~ /^xSLT/;
    return $ret;
}

1;
