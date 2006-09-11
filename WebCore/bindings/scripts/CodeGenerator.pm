# 
# KDOM IDL parser
#
# Copyright (C) 2005 Nikolas Zimmermann <wildfox@kde.org>
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
# 
# This file is part of the KDE project
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
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
# 

package CodeGenerator;

my $useDocument = "";
my $useGenerator = "";
my $useOutputDir = "";
my $useDirectories = "";
my $useLayerOnTop = 0;

my $codeGenerator = 0;

my %primitiveTypeHash = ("int" => 1, "short" => 1, "long" => 1, 
                         "unsigned int" => 1, "unsigned short" => 1,
                         "unsigned long" => 1, "float" => 1,
                         "double" => 1, "boolean" => 1, "void" => 1);

my %svgAnimatedTypeHash = ("SVGAnimatedAngle" => 1, "SVGAnimatedBoolean" => 1,
                           "SVGAnimatedEnumeration" => 1, "SVGAnimatedInteger" => 1,
                           "SVGAnimatedLength" => 1, "SVGAnimatedNumber" => 1,
                           "SVGAnimatedPreserveAspectRatio" => 1,
                           "SVGAnimatedRect" => 1, "SVGAnimatedString" => 1);
    
# Helpers for 'ScanDirectory'
my $endCondition = 0;
my $foundFilename = "";
my @foundFilenames = ();

# Default constructor
sub new
{
    my $object = shift;
    my $reference = { };

    $useDirectories = shift;
    $useGenerator = shift;
    $useOutputDir = shift;
    $useLayerOnTop = shift;

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
    my $defines = shift;
  
    my $ifaceName = "CodeGenerator" . $useGenerator;

    # Dynamically load external code generation perl module
    require $ifaceName . ".pm";
    $codeGenerator = $ifaceName->new($object, $useOutputDir, $useLayerOnTop);
    unless (defined($codeGenerator)) {
        my $classes = $useDocument->classes;
        foreach my $class (@$classes) {
            print "Skipping $useGenerator code generation for IDL interface \"" . $class->name . "\".\n";
        }
        return;
    }

    # Start the actual code generation!
    $codeGenerator->GenerateModule($useDocument, $defines);

    my $classes = $useDocument->classes;
    foreach my $class (@$classes) {
        print "Generating $useGenerator bindings code for IDL interface \"" . $class->name . "\"...\n";
        $codeGenerator->GenerateInterface($class, $defines);
    }

    $codeGenerator->finish();
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

    # Exception: For the DOM 'Node' is our topmost baseclass, not EventTargetNode.
    return if $parentsMax eq 1 and $parents[0] eq "EventTargetNode";

    my $ignoreParent = 1;
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
            print "  |  |>  Parsing parent IDL \"$foundFilename\" for interface \"$interface\"\n";

            # Step #2: Parse the found IDL file (in quiet mode).
            my $parser = IDLParser->new(1);
            my $document = $parser->Parse($foundFilename, "");

            foreach my $class (@{$document->classes}) {
                # Step #3: Collect constants & functions & attributes of this parent-class
                my $constantsMax = @{$class->constants};
                my $functionsMax = @{$class->functions};
                my $attributesMax = @{$class->attributes};

                print "  |  |>  -> Inherting $constantsMax constants, $functionsMax functions, $attributesMax attributes...\n  |  |>\n";

                # Step #4: Concatenate data
                foreach (@{$class->constants}) {
                    push(@$constantsRef, $_);
                }

                foreach (@{$class->functions}) {
                    push(@$functionsRef, $_);
                }

                foreach (@{$class->attributes}) {
                    push(@$attributesRef, $_);
                }

                # Step #4: Enter recursive parent search
                AddMethodsConstantsAndAttributesFromParentClasses($object, $class);
            }
        } else {
            die("Could NOT find specified parent interface \"$interface\"!\n");
        }
    }
}

# Helper for all CodeGenerator***.pm modules
sub IsPrimitiveType
{
    my $object = shift;
    my $type = shift;

    return 1 if ($primitiveTypeHash{$type});
    return 0;
}

sub IsSVGAnimatedType
{
    my $object = shift;
    my $type = shift;

    return 1 if ($svgAnimatedTypeHash{$type});
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

    chdir($directory) or die "[ERROR] Can't enter directory $directory: \"$!\"\n";
    opendir(DIR, ".") or die "[ERROR] Can't open directory $directory: \"$!\"\n";

    my @names = readdir(DIR) or die "[ERROR] Cant't read directory $directory: \"$!\"\n";
    closedir(DIR);

    foreach my $name (@names) {
        # Skip if we already found the right file or
        # if we encounter 'exotic' stuff (ie. '.', '..', '.svn')
        next if ($endCondition eq 1) or ($name =~ /^\./);

        # Recurisvely enter directory
        if (-d $name) {
            $object->ScanDirectory($interface, $name, $useDirectory, $reportAllFiles);
            next;
        }

        # Check wheter we found the desired file
        my $condition = ($name eq $interface);
        $condition = 1 if ($interface eq "allidls") and ($name =~ /\.idl$/);

        if ($condition) {
            $foundFilename = "$directory/$name";

            if ($reportAllFiles eq 0) {
                $endCondition = 1;
            } else {
                push(@foundFilenames, $foundFilename);
            }
        }

        chdir($useDirectory) or die "[ERROR] Can't change directory to $useDirectory: \"$!\"\n";
    }
}

1;
