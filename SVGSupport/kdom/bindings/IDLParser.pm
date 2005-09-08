#
# KDOM IDL parser
#
# Copyright (c) 2005 Nikolas Zimmermann <wildfox@kde.org>
#
package IDLParser;

use IDLStructure;

use constant MODE_UNDEF		=> 0; # Default mode.

use constant MODE_MODULE	=> 10; # 'module' section
use constant MODE_INTERFACE	=> 11; # 'interface' section
use constant MODE_EXCEPTION	=> 12; # 'exception' section

# Helper variables
my @temporaryContent = "";

my $parseMode = MODE_UNDEF;
my $preservedParseMode = MODE_UNDEF;

my $beQuiet; # Should not display anything on STDOUT?
my $document = 0; # Will hold the resulting 'idlDocument'

# Default Constructor
sub new
{
	my $object = shift;
	my $reference = { };

	$document = 0;
	$beQuiet = shift;

	bless($reference, $object);
	return $reference;
}

# Returns the parsed 'idlDocument'
sub Parse
{
	my $object = shift;
	my $url = shift;

	print " | *** Starting to parse $url...\n |\n" if(!$beQuiet);
	open(FILE, "<$url") || die "Couldn't open requested file (file: $url)!";
	my @documentContent = <FILE>;
	close(FILE);

	# Remove all comments, pleasing our parsing engine a lot...
	my $documentData = join("#", @documentContent);
	$documentData =~ s/\/\*(.|[\n\r])*?\*\///g;	# /* ... */ style comments
	$documentData =~ s/\/\/[^\n\r]*//g;			# // ...... style comments
	@documentContent = split("#", $documentData);

	my $dataAvailable = 0;

	# Simple IDL Parser (tm)
	foreach(@documentContent) {
		my $newParseMode = $object->DetermineParseMode($_);

		if($newParseMode ne MODE_UNDEF) {
			if($dataAvailable eq 0) {
				$dataAvailable = 1; # Start node building...
			} else {
				$object->ProcessSection();
			}
		}

		# Update detected data stream mode...
		if($newParseMode ne MODE_UNDEF) {
			$parseMode = $newParseMode;
		}

		push(@temporaryContent, $_);
	}

	# Check if there is anything remaining to parse...
	if(($parseMode ne MODE_UNDEF) and ($#temporaryContent > 0)) {
		$object->ProcessSection();
	}

	print " | *** Finished parsing!\n" if(!$beQuiet);
	return $document;
}

sub ParseModule
{
	my $object = shift;
	my $dataNode = shift;

	print " |- Trying to parse module...\n" if(!$beQuiet);

	my $data = join("", @temporaryContent);
	$data =~ /$IDLStructure::moduleSelector/;

	my $moduleName = (defined($1) ? $1 : die("Parsing error!\nSource:\n$data\n)"));
	$dataNode->module($moduleName);

	print "  |----> Module; NAME \"$moduleName\"\n |-\n |\n" if(!$beQuiet);
}

sub ParseInterface
{
	my $object = shift;
	my $dataNode = shift;
	my $sectionName = shift;

	my $data = join("", @temporaryContent);

	# Look for end-of-interface mark
	$data =~ /};/g;
	$data = substr($data, index($data, $sectionName), pos($data) - length($data));

	$data =~ s/[\n\r]//g;

	# Beginning of the regexp parsing magic
	if($sectionName eq "exception") {
		print " |- Trying to parse exception...\n" if(!$beQuiet);

		my $exceptionName = ""; my $exceptionData = "";
		my $exceptionDataName = ""; my $exceptionDataType = "";
	
		# Match identifier of the exception, and enclosed data...
		$data =~ /$IDLStructure::exceptionSelector/;
		$exceptionName = (defined($1) ? $1 : die("Parsing error!\nSource:\n$data\n)"));
		$exceptionData = (defined($2) ? $2 : die("Parsing error!\nSource:\n$data\n)"));

		('' =~ /^/); # Reset variables needed for regexp matching

		# ... parse enclosed data (get. name & type)
		$exceptionData =~ /$IDLStructure::exceptionSubSelector/;
		$exceptionDataType = (defined($1) ? $1 : die("Parsing error!\nSource:\n$data\n)"));
		$exceptionDataName = (defined($2) ? $2 : die("Parsing error!\nSource:\n$data\n)"));

		# Fill in domClass datastructure
		$dataNode->name($exceptionName);

		my $newDataNode = new domAttribute();
		$newDataNode->type("readonly attribute");
		$newDataNode->signature(new domSignature());

		$newDataNode->signature->name($exceptionDataName);
		$newDataNode->signature->type($exceptionDataType);
		$newDataNode->signature->hasPtrFlag(0);

		my $arrayRef = $dataNode->attributes;
		push(@$arrayRef, $newDataNode);

		print "  |----> Exception; NAME \"$exceptionName\" DATA TYPE \"$exceptionDataType\" DATA NAME \"$exceptionDataName\"\n |-\n |\n" if(!$beQuiet);
	} elsif($sectionName eq "interface") {
		print " |- Trying to parse interface...\n" if(!$beQuiet);

		my $interfaceName = "";
		my $interfaceData = "";
		
		# Match identifier of the interface, and enclosed data...
		$data =~ /$IDLStructure::interfaceSelector/;
		$interfaceNoDPtrFlag = (defined($1) ? $1 : " "); chop($interfaceNoDPtrFlag);
		$interfaceName = (defined($2) ? $2 : die("Parsing error!\nSource:\n$data\n)"));
		$interfaceBase = (defined($3) ? $3 : "");
		$interfaceData = (defined($4) ? $4 : die("Parsing error!\nSource:\n$data\n)"));

		# Fill in known parts of the domClass datastructure now...
		$dataNode->name($interfaceName);
		$dataNode->noDPtrFlag(($interfaceNoDPtrFlag =~ /$IDLStructure::nodptrSyntax/));

		# Inheritance detection
		my @interfaceParents = split(/,/, $interfaceBase);
		foreach(@interfaceParents) {
			my $line = $_;
			$line =~ s/\s*//g;

			my $arrayRef = $dataNode->parents;
			push(@$arrayRef, $line);
		}

		$interfaceData =~ s/[\n\r]//g;
		my @interfaceMethods = split(/;/, $interfaceData);

		foreach(@interfaceMethods) {
			my $line = $_;

			if($line =~ /attribute/) {
				$line =~ /$IDLStructure::interfaceAttributeSelector/;

				my $attributeType = (defined($1) ? $1 : die("Parsing error!\nSource:\n$line\n)"));
				my $attributePtrFlag = (defined($2) ? $2 : " "); chop($attributePtrFlag);
				my $attributeDataType = (defined($3) ? $3 : die("Parsing error!\nSource:\n$line\n)"));
				my $attributeDataName = (defined($4) ? $4 : die("Parsing error!\nSource:\n$line\n)"));
					
				('' =~ /^/); # Reset variables needed for regexp matching
				
				$line =~ /$IDLStructure::raisesSelector/;
				my $attributeException = (defined($1) ? $1 : "");
			
				my $newDataNode = new domAttribute();
				$newDataNode->type($attributeType);
				$newDataNode->signature(new domSignature());

				$newDataNode->signature->name($attributeDataName);
				$newDataNode->signature->type($attributeDataType);
				$newDataNode->signature->hasPtrFlag(($attributePtrFlag =~ /$IDLStructure::ptrSyntax/));

				my $arrayRef = $dataNode->attributes;
				push(@$arrayRef, $newDataNode);

				print "  |  |>  Attribute; TYPE \"$attributeType\" DATA NAME \"$attributeDataName\" DATA TYPE \"$attributeDataType\" EXCEPTION? \"$attributeException\" PTR FLAG \"$attributePtrFlag\"\n" if(!$beQuiet);

				$attributeException =~ s/\s+//g;
				@{$newDataNode->raisesExceptions} = split(/,/, $attributeException);
			} elsif(($line !~ s/^\s*$//g) and ($line !~ /^\s+const/)) {
				$line =~ /$IDLStructure::interfaceMethodSelector/;

				my $methodPtrFlag = (defined($1) ? $1 : " "); chop($methodPtrFlag);
				my $methodType = (defined($2) ? $2 : die("Parsing error!\nSource:\n$line\n)"));
				my $methodName = (defined($3) ? $3 : die("Parsing error!\nSource:\n$line\n)"));
				my $methodSignature = (defined($4) ? $4 : die("Parsing error!\nSource:\n$line\n)"));
				
				('' =~ /^/); # Reset variables needed for regexp matching
				
				$line =~ /$IDLStructure::raisesSelector/;
				my $methodException = (defined($1) ? $1 : "");

				my $newDataNode = new domFunction();
				$newDataNode->signature(new domSignature());

				$newDataNode->signature->name($methodName);
				$newDataNode->signature->type($methodType);
				$newDataNode->signature->hasPtrFlag(($methodPtrFlag =~ /$IDLStructure::ptrSyntax/));

				print "  |  |-  Method; TYPE \"$methodType\" NAME \"$methodName\" EXCEPTION? \"$methodException\" PTR FLAG \"$methodPtrFlag\"\n" if(!$beQuiet);

				$methodException =~ s/\s+//g;
				@{$newDataNode->raisesExceptions} = split(/,/, $methodException);

				my @params = split(/,/, $methodSignature);
				foreach(@params) {
					my $line = $_;

					$line =~ /$IDLStructure::interfaceParameterSelector/;
					my $paramPtrFlag = (defined($1) ? $1 : " "); chop($paramPtrFlag);
					my $paramType = (defined($2) ? $2 : die("Parsing error!\nSource:\n$line\n)"));
					my $paramName = (defined($3) ? $3 : die("Parsing error!\nSource:\n$line\n)"));

					my $paramDataNode = new domSignature();
					$paramDataNode->name($paramName);
					$paramDataNode->type($paramType);
					$paramDataNode->hasPtrFlag(($paramPtrFlag =~ /$IDLStructure::ptrSyntax/));

					my $arrayRef = $newDataNode->parameters;
					push(@$arrayRef, $paramDataNode);

					print "  |   |>  Param; TYPE \"$paramType\" NAME \"$paramName\" PTR FLAG \"$paramPtrFlag\"\n" if(!$beQuiet);
				}

				my $arrayRef = $dataNode->functions;
				push(@$arrayRef, $newDataNode);
			} elsif($line =~ /^\s+const/) {
				$line =~ /$IDLStructure::constantSelector/;
				my $constType = (defined($1) ? $1 : die("Parsing error!\nSource:\n$line\n)"));
				my $constName = (defined($2) ? $2 : die("Parsing error!\nSource:\n$line\n)"));
				my $constValue = (defined($3) ? $3 : die("Parsing error!\nSource:\n$line\n)"));

				my $newDataNode = new domConstant();
				$newDataNode->name($constName);
				$newDataNode->type($constType);
				$newDataNode->value($constValue);

				my $arrayRef = $dataNode->constants;
				push(@$arrayRef, $newDataNode);

				print "  |   |>  Constant; TYPE \"$constType\" NAME \"$constName\" VALUE \"$constValue\"\n" if(!$beQuiet);
			}
		}

		print "  |----> Interface; NAME \"$interfaceName\"\n |-\n |\n" if(!$beQuiet);
	}
}

# Internal helper
sub DetermineParseMode
{
	my $object = shift;	
	my $line = shift;

	my $mode = MODE_UNDEF;
	if($_ =~ /module/) {
		$mode = MODE_MODULE;
	} elsif($_ =~ /interface/) {
		$mode = MODE_INTERFACE;
	} elsif($_ =~ /exception/) {
		$mode = MODE_EXCEPTION;
	}

	return $mode;
}

# Internal helper
sub ProcessSection
{
	my $object = shift;
	
	if($parseMode eq MODE_MODULE) {
		die ("Two modules in one file! Fatal error!\n") if($document ne 0);
		$document = new idlDocument();
		$object->ParseModule($document);
	} elsif($parseMode eq MODE_INTERFACE) {
		my $node = new domClass();
		$object->ParseInterface($node, "interface");
		
		die ("No module specified! Fatal Error!\n") if($document eq 0);
		my $arrayRef = $document->classes;
		push(@$arrayRef, $node);
	} elsif($parseMode eq MODE_EXCEPTION) {
		my $node = new domClass();
		$object->ParseInterface($node, "exception");

		die ("No module specified! Fatal Error!\n") if($document eq 0);
		my $arrayRef = $document->classes;
		push(@$arrayRef, $node);
	}

	@temporaryContent = "";
}

1;
