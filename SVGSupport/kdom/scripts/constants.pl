#!/usr/bin/perl -w

my $rawfile = shift;

my $resultfile = `pwd`;
chop $resultfile;
$resultfile = $resultfile . "/" . $rawfile;

my $useNameSpace = $ARGV[1];

# checks to see if an item is in the specified array
# taken from Array::PAT - Copyright (c) 2003 Brian Shade <bshade@eden.rutgers.edu>
sub in_array {
	my ($check, @array) = @_;
	my $retval = 0;
	foreach my $test (@array) {
		if($check eq $test) {
			$retval =  1;
		}
	}
	return $retval;
}

# remove all duplicate entries in an array
# taken from Array::PAT - Copyright (c) 2003 Brian Shade <bshade@eden.rutgers.edu>
sub array_unique {
	my (@array) = @_;
	my @nodupes;
	foreach my $element (@array) {
		if(in_array($element, @nodupes) == 0) {
			@nodupes = (@nodupes, $element);
		}
	}
	return @nodupes;
}

# Get list of classes containg ecma definitions
my $found = `find -maxdepth 1 -type f | xargs grep \@begin | grep "\.cc" | cut -c3-`;
my @result = split(/\n/, $found);

foreach(@result) {
	$_ = substr($_, 0, index($_, ".cc"));
}

# Remove duplicated - happens when for example both
# Node and NodeProto classes are defined in the source file
@result = array_unique(@result);

print STDOUT "Generating ecma constants..:\n";

$rawfile =~ s/data\///;
$rawfile =~ s/\.h//;
$rawfile =~ s/\.\.\///;
$rawfile =~ tr/[a-z]/[A-Z]/;
my $fileheader = "/* Automatically generated using kdom/scripts/constants.pl. DO NOT EDIT ! */\n\n#ifndef ${useNameSpace}_$rawfile" . "_H\n#define ${useNameSpace}_$rawfile" . "_H\n\nnamespace ${useNameSpace}\n{";

# Open output file for writing
open(OUTPUT, ">$resultfile") || die "[failed]\n";
print OUTPUT $fileheader;

my $first = 1;

foreach(@result) {
	my $line = $_;
	
	print STDOUT "Class: $line ";
	open(FILE, $line . ".cc") or die("failed\n");
	 
	my @contents = <FILE>;
	chomp @contents;
	close FILE;

	# Filter out the relevant parts
	my $data = "";
	my $get = 0;
	foreach(@contents) {
		my $stream = $_;
		if($stream =~ /\@begin/) {
			if($stream =~ /Constructor/) {
				$get = 0; next;
			} else {
				$get = 1; next;
			}
		} elsif($stream =~ /#/) { # Skip lines containting comments! Also removes lines containing trailing comments!
			next;		
		} elsif($stream =~ /\@end/) {
			$get = 0;
		}

		if($get eq 1) {
			$data = $data . $stream . "\n";
		}
	}

	# Skip cases, when no data has been "filtered" out
	if(length($data) eq 0) {
		print STDOUT "[skipped]\n";
		next;
	}

	# Formatting stuff...
	if($first eq 1) {
		$first = 0;
		print OUTPUT "\n";
	} else {
		print OUTPUT "\n\n";
	}

	# Transform to c++ type definitions
	my @types = "";
	
	# Collect properties/function definitions
	@contents = split(/\n/, $data);
	foreach(@contents) {
		my $stream = $_;

		if(length($stream) ne 0) {
			$stream = substr($stream, 1, index($stream, "\t"));
			$stream =~ s/\s+/ /g;
			$stream =~ s/-([A-Za-z])/uc $1/ge;
			$stream = ucfirst($stream);	
			chop $stream;

			push(@types, $stream);
		}
	}
	@types = array_unique(@types);

	# Begin new enum container
	print OUTPUT "\tstruct $line" . "Constants\n\t{\n\t\ttypedef enum\n\t\t{\n\t\t\t";

	my $i = -1;
	my $typesCount = @types;
	foreach(@types) {
		$i++;
		# As always in my perl scripts: a hack :)
		if($i eq 6 || $i eq 12 || $i eq 18 || $i eq 24 || $i eq 30 ||
		   $i eq 36 || $i eq 42) {
			print OUTPUT "\n\t\t\t";
		}
	
		if($i eq 0) {
			print OUTPUT $_;
		} elsif($i eq $typesCount - 1) {
			print OUTPUT $_ . " ";
		} else {
			print OUTPUT $_ . ", ";
		}
	}
	
	# End of enum container
	print OUTPUT "\n\t\t} $line" . "ConstantsEnum;\n\t};";
	print STDOUT "[done]\n";
}

print OUTPUT "\n};\n\n#endif\n";
close OUTPUT;
