#!/usr/bin/perl -w
#
# KDOM w3c Documentation Parser
#
# Copyright (c) 2005 Rob Buis <buis@kde.org>
#
package DocParser;

use XML::Parser;
use IDLStructure;

my $cur_target = "";
my $cur_attr_target = "";
my $cur_method_target = "";
my $cur_exception = "";
my $cur_exception_data = "";
my $cur_group = "";
my $cur_data = "";
my $dataNode = "";

# Default constructor
sub new
{
	my $object = shift;
	my $reference = { };

	my $file = shift;
	$dataNode = shift;

	if(-f $file) {
		my $parser = new XML::Parser(ErrorContext => 2);
		$parser->setHandlers(Char => \&char_handler, Start => \&start_handler, End => \&end_handler);
		$parser->parsefile($file);
	} else { # Initialize documentation variables as they remain unset...
		foreach(@{$dataNode->functions}) {
			$_->documentation("");
		}

		foreach(@{$dataNode->attributes}) {
			$_->documentation();
			$_->exceptionDocumentation();
		}

		foreach(@{$dataNode->constants}) {
			$_->documentation("");
		}

		$dataNode->documentation("");
	}

	bless($reference, $object);
	return $reference;
}

sub trim
{
	my $string = shift;
	for($string) {
		s/^\s+//;
		s/\s+$//;
	}

	return $string;
}

sub char_handler
{
	my ($p, $data) = @_;
	if ($cur_exception ne "") {
		$cur_exception_data .= ($data);
	} else {
		$cur_data .= ($data);
	}
}

sub start_handler
{
	my $p = shift;
	my $data = shift;

	if($data =~ /interface/) {
		$cur_exception = "";
		$cur_exception_data = "";
		while(@_) {
			my $att = shift;
			my $val = shift;

			if($att =~ /name/) {
				$name = $dataNode->name; print "NEW NAME $name VALUE $val CUR TAR $cur_target\n";
				if($val =~ /$name/) {
					$cur_target = $val;
					print "SETTING CURRENT TARGET TO $val\n";
				}
			}
		}
	} elsif($data =~ /attribute/) {
		$cur_exception = "";
		$cur_exception_data = "";
		while(@_) {
			my $att = shift;
			my $val = shift;

			if($att =~ /name/) {
				$cur_attr_target = $val;
				print "SETTING ATTR CURRENT TARGET TO $val\n" if($cur_target ne "");
			}
		}
	} elsif($data =~ /method/) {
		$cur_exception = "";
		$cur_exception_data = "";
		while(@_) {
			my $att = shift;
			my $val = shift;

			if($att =~ /name/) {
				$cur_method_target = $val;
				print "SETTING METHOD CURRENT TARGET TO $val\n" if($cur_target ne "");
			}
		}
	} elsif ($data =~ /exception/) {
		while(@_) {
			my $att = shift;
			my $val = shift;

			if($att =~ /name/) {
				$cur_exception = $val;
				print "SETTING EXCEPTION TO $val\n";
			}
		}
	} elsif($data =~ /descr/) {
		$cur_data = '';
	} elsif ($data =~ /code/) {
		$cur_data .= '<code>';
	} elsif ($data =~ /group/) {
		$cur_group = 'dummy';
	}
}

sub setAttributeDocumentation
{
	my $attrname = shift;
	my $data = shift;

	print " -> ADD ATTR DOC FOR $attrname ($cur_exception_data)\n";

	foreach(@{$dataNode->attributes}) {
		my $attribute = $_;

		if($attribute->signature->name eq $attrname) {
			my @lines = split(/\n/, $data);

			foreach (@lines) {
				my $arrayRef = $attribute->documentation;
				push(@$arrayRef, $_);
			}
			$attribute->exceptionName($cur_exception);
			if ($cur_exception_data ne "") {
				my @lines = split(/\n/, $cur_exception_data);
				foreach (@lines) {
					my $arrayRef = $attribute->exceptionDocumentation;
					push(@$arrayRef, $_);
				}
			}
		}
	}
}

sub setFunctionDocumentation
{
	my $functionname = shift;
	my $data = shift;

	print " -> ADD ATTR DOC FOR $functionname\n";

	foreach(@{$dataNode->functions}) {
		my $function = $_;

		if($function->signature->name eq $functionname) {
			$function->documentation($data);
		}
	}
}

sub end_handler
{
	my ($p, $data) = @_;
	$name = $dataNode->name;

	if($cur_target eq $name) {
		if($data =~ /descr/) {
			if($cur_attr_target ne "") {
				setAttributeDocumentation($cur_attr_target, trim($cur_data));
			} elsif ($cur_method_target ne "") {
				setFunctionDocumentation($cur_method_target, trim($cur_data));
			} else {
				$dataNode->documentation($cur_data);
			}
			$cur_exception = '';
			$cur_data = '';
		} elsif ($data =~ /method/) {
			$cur_method_target = '';
		} elsif ($data =~ /attribute/) {
			$cur_attr_target = '';
		} elsif ($data =~ /group/) {
			$cur_group = '';
		} elsif ($data =~ /interface/) {
			$cur_target = "dummy";
		} elsif ($data =~ /code/) {
			$cur_data .= '</code>';
		}
	}
}

1;
