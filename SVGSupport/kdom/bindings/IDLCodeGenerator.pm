#
# KDOM IDL parser
#
# Copyright (c) 2005 Nikolas Zimmermann <wildfox@kde.org>
#
package IDLCodeGenerator;

my $useDocument = "";
my $useGenerator = "";
my $useOutputDir = "";
my $useDirectories = "";
my $useDocumentation = "";
my $useLayerOnTop = 0;

my $codeGenerator = 0;

# Used to map between modules & namespaces
my %moduleNamespaceHash;

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
	$useDocumentation = shift;
	$useLayerOnTop = shift;

	bless($reference, $object);
	return $reference;
}

sub ProcessDocument
{
	my $object = shift;
	$useDocument = shift;

	my $ifaceName = $useGenerator;
	$ifaceName =~ s/\b(\w)/\U$1/g; # Make first letter of each word uppercase
	$ifaceName = "IDLCodeGenerator$ifaceName";

	# Dynamically load external code generation perl module...
	require $ifaceName . ".pm";
	$codeGenerator = $ifaceName->new($object, $useOutputDir, $useDocumentation, $useLayerOnTop);
 
	print " | *** Starting to generate code using \"$ifaceName\"...\n |\n";

	# Start the actual code generation!
	$codeGenerator->GenerateModule($useDocument);

	my $arrayRef = $useDocument->classes;
	foreach(@$arrayRef) {
		my $class = $_;

		print " |- Processing interface \"" . $class->name . "\"...\n";

		$codeGenerator->GenerateInterface($class);
	}

	$codeGenerator->finish();

	print " | *** Finished generation!\n";
}

# Helper for all IDLCodeGenerator***.pm modules
sub FindTopBaseClass
{
	# If you are processing the 'Attr' interface, it has the single
	# parent interface 'Node', which is the topmost base class. Return it.
	#
	# It gets trickier for ie. the 'MouseEvent' interface, whose parent is
	# the 'UIEvent' interface, whose parent is the 'Event' interface. Return it.
	my $object = shift;

	my $interface = shift;
	$interface =~ s/[a-zA-Z0-9]*:://; # Strip module.

	my $topBaseClass = "";
	
	# Loop until we found the top most base class for 'interface'
	while($interface ne "") {
		# Step #1: Find the IDL file associated with 'interface'
		$endCondition = 0; $foundFilename = "";

		foreach(@{$useDirectories}) {
			if($foundFilename eq "") {
				$object->ScanDirectory("$interface.idl", $_, $_, 0);
			}
		}

		if($foundFilename ne "") {
			print "  |  |>  Parsing parent IDL \"$foundFilename\" for interface \"$interface\"\n";

			# Step #2: Parse the found IDL file (in quiet mode).
			my $parser = IDLParser->new(1);
			my $document = $parser->Parse($foundFilename);

			# Step #3: Check wheter the parsed IDL file has parents
			foreach(@{$document->classes}) {
				my $class = $_;

				my $useInterface = $interface;

				if($class->name eq $useInterface) {
					my @parents = @{$class->parents};
					my $parentsMax = @{$class->parents};

					$interface = "";

					# Exception: For the DOM 'Node' is our topmost baseclass, not EventTarget.
					if(($parentsMax > 0) and ($parents[0] ne "events::EventTarget")) {
						$interface = $parents[0];
						$interface =~ s/[a-zA-Z0-9]*:://; # Strip module;
					} elsif(!$class->noDPtrFlag) { # Include 'module' ...
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

	my $interface = shift;
	$interface =~ s/[a-zA-Z0-9]*:://; # Strip module.

	my $hasWriteableAttributes = 0;
	
	# Step #1: Find the IDL file associated with 'interface'
	$endCondition = 0; $foundFilename = "";

	foreach(@{$useDirectories}) {
		if($foundFilename eq "") {
			$object->ScanDirectory("$interface.idl", $_, $_, 0);
		}
	}

	# Step #2: Parse the found IDL file (in quiet mode).
	my $parser = IDLParser->new(1);
	my $document = $parser->Parse($foundFilename);

	# Step #3: Check wheter the parsed IDL file has parents
	foreach(@{$document->classes}) {
		my $class = $_;

		if($class->name eq $interface) {
			foreach(@{$class->attributes}) {
				if($_->type !~ /^readonly\ attribute$/) {
					$hasWriteableAttributes = 1;
				}
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
	foreach(@{$useDirectories}) {
		$endCondition = 0;
		@foundFilenames = ();

		$object->ScanDirectory("allidls", $_, $_, 1);
		foreach(@foundFilenames) {
			push(@allIDLFiles, $_);
		}
	}

	# Step #2: Loop through all found IDL files...
	my %classDataCache;
	foreach(@allIDLFiles) {
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
	while(my($name, $document) = each %classDataCache) {
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
	foreach(@{$useDirectories}) {
		$endCondition = 0;
		@foundFilenames = ();

		$object->ScanDirectory("allidls", $_, $_, 1);
		foreach(@foundFilenames) {
			push(@allIDLFiles, $_);
		}
	}

	# Step #2: Loop through all found IDL files...
	my @classList = ();
	foreach(@allIDLFiles) {
		# Step #3: Parse the found IDL file (in quiet mode).
		my $parser = IDLParser->new(1);
		my $document = $parser->Parse($_);

		# Step #4: Check if class is a baseclass...
		foreach(@{$document->classes}) {
			my $class = $_;

			my $identifier = $class->name;
			my $namespace = $moduleNamespaceHash{$document->module};
			$identifier = $namespace . "::" . $identifier if($namespace ne "");

			my @array = grep { /^$identifier$/ } @$classList;

			my $arraySize = @array;
			if($arraySize eq 0) {
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

	if($endCondition eq 1) {
		return 1;
	}

	foreach(@{$document->classes}) {
		my $class = $_;

		foreach(@{$class->parents}) {
			my $cacheHandle = $_;
			$cacheHandle =~ s/[a-zA-Z0-9]*:://; # Strip module.

			if($cacheHandle eq $interface) {
				my $identifier = $document->module . "::" . $class->name;
				my @array = grep { /^$identifier$/ } @$classList; my $arraySize = @array;
				push(@$classList, $identifier) if($arraySize eq 0);

				$endCondition = 1;
				return $endCondition;
			} else { 
				my %cache = %{$classDataCache};

				my $checkDocument = $cache{$cacheHandle};
				$endCondition = $object->RecursiveInheritanceHelper($checkDocument, $interface,
																	$classList, $classDataCache);
				if($endCondition eq 1) {
					my $identifier = $document->module . "::" . $class->name;
					my @array = grep { /^$identifier$/ } @$classList; my $arraySize = @array;
					push(@$classList, $identifier) if($arraySize eq 0);

					return $endCondition;
				}
			}
		}
	}

	return $endCondition;
}

# Helper for all IDLCodeGenerator***.pm modules
sub ModuleNamespaceHash
{
	return \%moduleNamespaceHash;
}

# Helper for all IDLCodeGenerator***.pm modules
sub CreateModuleNamespaceHash
{
	my $object = shift;

	# Loop through all included directories to scan for 'kdomdefs.idl' files...
	my @allDefFiles = ();
	foreach(@{$useDirectories}) {
		@foundFilenames = ();
		$object->ScanDirectory("kdomdefs.idl", $_, $_, 1);
		foreach(@foundFilenames) {
			push(@allDefFiles, $_);
		}
	}

	foreach(@allDefFiles) {
		open(FILE, $_) || die "Couldn't open requested file $_!";
		my $data = join("", <FILE>);
		close(FILE);

		$data =~ /$IDLStructure::moduleNSSelector/;

		my $moduleName = (defined($1) ? $1 : die("Parsing error!\nSource:\n$data\n)"));
		my $moduleNamespace = (defined($2) ? $2 : die("Parsing error!\nSource:\n$data\n)"));

		$moduleNamespaceHash{$moduleName} = $moduleNamespace;
	}
}

# Helper for all IDLCodeGenerator***.pm modules
sub GenerateInclude
{
	my $object = shift;

	my %ret = $object->ExtractNamespace($_, 0, "");

	my $includeFile = $ret{'namespace'};
	$includeFile = "kdom/bindings/js/" . $includeFile if($includeFile ne "");
	$includeFile .= "/" if($includeFile ne "");
	$includeFile .= $ret{'type'};

	return $includeFile;
}

# Helper for all IDLCodeGenerator***.pm modules
sub SplitNamespaces
{
	my $object = shift;

	my @namespaces = split(/\:\:/, shift);
	return \@namespaces;
}	

# Helper for all IDLCodeGenerator***.pm modules
sub ExtractNamespace
{
	my $object = shift;

	my $typeString = shift;
	my $processTypes = shift;
	my $fallbackNS = shift;

	# Extract type identifier without namespace.
	$typeString =~ /$IDLStructure::typeNamespaceSelector/;
    my $namespace = (defined($1) ? $1 : die("Type extraction error!\nSource:\n$typeString\n)"));
    my $type = (defined($2) ? $2 : die("Namespace extraction error!\nSource:\n$typeString\n)"));

	if($namespace ne "") { # Assume current module...
		# 'dom::' -> 'dom' (or 'KDOM::' -> 'KDOM')
		chop($namespace); chop($namespace);
	}

	if(defined($processTypes) and ($processTypes eq 1)) { # Just extract or transform module -> namespace?
		if(!$object->IsPrimitiveType($type)) {
			if($namespace eq "") { # Assume current module...
				$namespace = $fallbackNS;
			} else { # Map 'module' to it's corresponding namespace...
				$namespace = $moduleNamespaceHash{$namespace};
				die("Invalid type specifier \"$typeString\"") if(!defined($namespace));
			}
		} else { # No namespace for primitives...
			$namespace = "";
		}
	}

	# Initialize resulting hash...
	my %ret = (type => $type, namespace => $namespace);
	return %ret;
}

# Helper for all IDLCodeGenerator***.pm modules
sub IsPrimitiveType
{
	my $object = shift;

	my $type = shift;

	if(($type =~ /^int$/) or ($type =~ /^short$/) or ($type =~ /^long$/) or
	   ($type =~ /^unsigned int$/) or ($type =~ /^unsigned short$/) or ($type =~ /^unsigned long$/) or
	   ($type =~ /^float$/) or ($type =~ /^double$/) or ($type =~ /^bool$/) or ($type =~ /^void$/)) {
		return 1;
	}

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

	if(($endCondition eq 1) and ($reportAllFiles eq 0)) {
		return;
	}

	chdir($directory) or die "[ERROR] Can't enter directory $directory: \"$!\"\n";
	opendir(DIR, ".") or die "[ERROR] Can't open directory $directory: \"$!\"\n";

	my @names = readdir(DIR) or die "[ERROR] Cant't read directory $directory: \"$!\"\n";
	closedir(DIR);

	foreach(@names) {
		my $name = $_;

		# Skip if we already found the right file or
		# if we encounter 'exotic' stuff (ie. '.', '..', '.svn')
		if(($endCondition eq 1) or ($name =~ /^\./)) {
			next;
		}

		# Recurisvely enter directory...
		if(-d $name) {
			$object->ScanDirectory($interface, $name, $useDirectory, $reportAllFiles);
			next;
		}

		# Check wheter we found the desired file...
		my $condition = ($name eq $interface);
		if(($interface eq "allidls") and
		   ($name !~ /kdomdefs/) and
		   ($name =~ /\.idl$/)) {
			$condition = 1;
		}

		if($condition) {
			$foundFilename = "$useDirectory/$directory/$name";

			if($reportAllFiles eq 0) {
				$endCondition = 1;
			} else {
				push(@foundFilenames, $foundFilename);
			}
		}

		chdir($useDirectory) or die "[ERROR] Can't change directory to $useDirectory: \"$!\"\n";
	}
}

1;
