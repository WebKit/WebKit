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

# Used to map between modules & namespaces
my %moduleNamespaceHash;

# Used to map between modules and their implementation namespaces
my %moduleImplementationNamespaceHash;

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

    # Dynamically load external code generation perl module...
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
    my $topBaseClass = "";

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
                # Step #3: Collect constants & functions & attributes of this parent-class...
                my $constantsMax = @{$class->constants};
                my $functionsMax = @{$class->functions};
                my $attributesMax = @{$class->attributes};

                print "  |  |>  -> Inherting $constantsMax constants, $functionsMax functions, $attributesMax attributes...\n  |  |>\n";

                # Step #4: Concatenate data...
                foreach (@{$class->constants}) {
                    push(@$constantsRef, $_);
                }

                foreach (@{$class->functions}) {
                    push(@$functionsRef, $_);
                }

                foreach (@{$class->attributes}) {
                    push(@$attributesRef, $_);
                }

                # Step #4: Enter recursive parent search...
                AddMethodsConstantsAndAttributesFromParentClasses($object, $class);
            }
        } else {
            die("Could NOT find specified parent interface \"$interface\"!\n");
        }
    }
}
 
# Helper for all CodeGenerator***.pm modules
sub FindTopBaseClass
{
    # If you are processing the 'Attr' interface, it has the single
    # parent interface 'Node', which is the topmost base class. Return it.
    #
    # It gets trickier for ie. the 'PlatformMouseEvent' interface, whose parent is
    # the 'UIEvent' interface, whose parent is the 'Event' interface. Return it.
    my $object = shift;
    my $interface = StripModule(shift);
    my $topBaseClass = "";
  
    # Loop until we found the top most base class for 'interface'
    while($interface ne "") {
        # Step #1: Find the IDL file associated with 'interface'
        $endCondition = 0; $foundFilename = "";

        foreach (@{$useDirectories}) {
            $object->ScanDirectory("$interface.idl", $_, $_, 0) if $foundFilename eq "";
        }

        if ($foundFilename ne "") {
            print "  |  |>  Parsing parent IDL \"$foundFilename\" for interface \"$interface\"\n";

            # Step #2: Parse the found IDL file (in quiet mode).
            my $parser = IDLParser->new(1);
            my $document = $parser->Parse($foundFilename);

            # Step #3: Check wheter the parsed IDL file has parents
            foreach my $class (@{$document->classes}) {
                my $useInterface = $interface;

                if ($class->name eq $useInterface) {
                    my @parents = @{$class->parents};
                    my $parentsMax = @{$class->parents};

                    $interface = "";

                    # Exception: For the DOM 'Node' is our topmost baseclass, not EventTarget.
                    if (($parentsMax > 0) and ($parents[0] ne "events::EventTarget")) {
                        $interface = StripModule($parents[0]);
                    } elsif (!$class->noDPtrFlag) { # Include 'module' ...
                        $topBaseClass = $document->module . "::" . $class->name;
                    }
                }
            }
        } else {
            die("Could NOT find specified parent interface \"$interface\"!\n");
        }
    }

    return $topBaseClass;
}

# Helper for all IDLCodeGenerator***.pm modules
sub ClassHasWriteableAttributes
{
    # Determine wheter a given interface has any writeable attributes...
    my $object = shift;
    my $interface = StripModule(shift);
    my $hasWriteableAttributes = 0;

    # Step #1: Find the IDL file associated with 'interface'
    $endCondition = 0;
    $foundFilename = "";

    foreach (@{$useDirectories}) {
        $object->ScanDirectory("$interface.idl", $_, $_, 0) if $foundFilename eq "";
    }

    # Step #2: Parse the found IDL file (in quiet mode).
    my $parser = IDLParser->new(1);
    my $document = $parser->Parse($foundFilename);

    # Step #3: Check wheter the parsed IDL file has parents
    foreach my $class (@{$document->classes}) {
        if ($class->name eq $interface) {
            foreach (@{$class->attributes}) {
                $hasWriteableAttributes = 1 if ($_->type ne "readonly attribute");
            }
        }
    }

    return $hasWriteableAttributes;
}

# Helper for all IDLCodeGenerator***.pm modules
sub AllClassesWhichInheritFrom
{
    # Determine which interfaces inherit from the passed one...
    my $object = shift;

    my $interface = shift;
    $interface =~ s/([a-zA-Z0-9]*::)*//; # Strip namespace(s).

    # Step #1: Loop through all included directories to scan for all IDL files...
    my @allIDLFiles = ();
    foreach (@{$useDirectories}) {
        $endCondition = 0;
        @foundFilenames = ();

        $object->ScanDirectory("allidls", $_, $_, 1);
        foreach (@foundFilenames) {
            push(@allIDLFiles, $_);
        }
    }

    # Step #2: Loop through all found IDL files...
    my %classDataCache;
    foreach (@allIDLFiles) {
        # Step #3: Parse the found IDL file (in quiet mode).
        my $parser = IDLParser->new(1);
        my $document = $parser->Parse($_);

        # Step #4: Cache the parsed IDL datastructures.
        my $cacheHandle = $_; $cacheHandle =~ s/.*\/(.*)\.idl//;
        $classDataCache{$1} = $document;
    }

    my %classDataCacheCopy = %classDataCache; # Protect!

    # Step #5: Loop through all cached IDL documents...
    my @classList = ();
    while (my($name, $document) = each %classDataCache) {
        $endCondition = 0;

        # Step #6: Check wheter the parsed IDL file has parents...
        $object->RecursiveInheritanceHelper($document, $interface, \@classList, \%classDataCacheCopy);
    }

    # Step #7: Return list of all classes which inherit from me!
    return \@classList;
}

# Helper for all IDLCodeGenerator***.pm modules
sub AllClasses
{
    # Determines all interfaces within a project...
    my $object = shift;

    # Step #1: Loop through all included directories to scan for all IDL files...
    my @allIDLFiles = ();
    foreach (@{$useDirectories}) {
        $endCondition = 0;
        @foundFilenames = ();

        $object->ScanDirectory("allidls", $_, $_, 1);
        foreach (@foundFilenames) {
            push(@allIDLFiles, $_);
        }
    }

    # Step #2: Loop through all found IDL files...
    my @classList = ();
    foreach (@allIDLFiles) {
        # Step #3: Parse the found IDL file (in quiet mode).
        my $parser = IDLParser->new(1);
        my $document = $parser->Parse($_);

        # Step #4: Check if class is a baseclass...
        foreach my $class (@{$document->classes}) {
            my $identifier = $class->name;
            my $namespace = $moduleNamespaceHash{$document->module};
            $identifier = $namespace . "::" . $identifier if $namespace ne "";

            my @array = grep { /^$identifier$/ } @$classList;

            my $arraySize = @array;
            if ($arraySize eq 0) {
                push(@classList, $identifier);
            }
        }
    }

    # Step #7: Return list of all base classes!
    return \@classList;
}

# Internal helper for 'AllClassesWhichInheritFrom'
sub RecursiveInheritanceHelper
{
    my $object = shift;

    my $document = shift;
    my $interface = shift;
    my $classList = shift;
    my $classDataCache = shift;

    return 1 if $endCondition eq 1;

    foreach my $class (@{$document->classes}) {
        foreach (@{$class->parents}) {
            my $cacheHandle = StripModule($_);

            if ($cacheHandle eq $interface) {
                my $identifier = $document->module . "::" . $class->name;
                my @array = grep { /^$identifier$/ } @$classList; my $arraySize = @array;
                push(@$classList, $identifier) if $arraySize eq 0;

                $endCondition = 1;
                return $endCondition;
            } else { 
                my %cache = %{$classDataCache};

                my $checkDocument = $cache{$cacheHandle};
                $endCondition = $object->RecursiveInheritanceHelper($checkDocument, $interface,
                                                                    $classList, $classDataCache);
                if ($endCondition eq 1) {
                    my $identifier = $document->module . "::" . $class->name;
                    my @array = grep { /^$identifier$/ } @$classList; my $arraySize = @array;
                    push(@$classList, $identifier) if $arraySize eq 0;

                    return $endCondition;
                }
            }
        }
    }

    return $endCondition;
}

# Helper for all CodeGenerator***.pm modules
sub RemoveExcludedAttributesAndFunctions
{
    my $object = shift;
    my $dataNode = shift;
    my $excludeLang = shift;

    my $i = 0;
    while ($i < @{$dataNode->attributes}) {
        my $lang = ${$dataNode->attributes}[$i]->signature->extendedAttributes->{"Exclude"};
        if ($lang and $lang eq $excludeLang) {
            splice(@{$dataNode->attributes}, $i, 1);
        } else {
            $i++;
        }
    }

    $i = 0;
    while ($i < @{$dataNode->functions}) {
        my $lang = ${$dataNode->functions}[$i]->signature->extendedAttributes->{"Exclude"};
        if ($lang and $lang eq $excludeLang) {
            splice(@{$dataNode->functions}, $i, 1);
        } else {
            $i++;
        }
    }
}

# Helper for all CodeGenerator***.pm modules
sub IsPrimitiveType
{
    my $object = shift;
    my $type = shift;

    return 1 if $type eq "int" or $type eq "short" or $type eq "long" 
                or $type eq "unsigned int" or $type eq "unsigned short"
                or $type eq "unsigned long" or $type eq "float" or $type eq "double"
                or $type eq "boolean" or $type eq "void";
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

        # Recurisvely enter directory...
        if (-d $name) {
            $object->ScanDirectory($interface, $name, $useDirectory, $reportAllFiles);
            next;
        }

        # Check wheter we found the desired file...
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
