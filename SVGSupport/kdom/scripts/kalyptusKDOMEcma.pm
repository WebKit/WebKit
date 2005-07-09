#***************************************************************************
#            kalyptusCxxToEMA.pm -  Generates class info for ECMA bindings in KDE
#                             -------------------
#    begin                : Fri Jan 25 12:00:00 2000
#    copyright            : (C) 2002 Lost Highway Ltd. All Rights Reserved.
#    email                : david@mandrakesoft.com
#    author               : David Faure.
#***************************************************************************/

#/***************************************************************************
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU General Public License as published by  *
# *   the Free Software Foundation; either version 2 of the License, or     *
# *   (at your option) any later version.                                   *
# *                                                                         *
#***************************************************************************/

package kalyptusKDOMEcma;

use File::Path;
use File::Basename;

use Carp;
use Ast;
use kdocAstUtil;
use kdocUtil;
use Iter;
use kalyptusDataDict;

use strict;
no strict "subs";

use vars qw/
	$libname $rootnode $outputdir $opt $debug
	%skippedClasses %hasHashTable %hasCast %hasFunctions %hasBridge %hasGet %hasPut/;

sub writeDoc
{
	( $libname, $rootnode, $outputdir, $opt ) = @_;

	print STDERR "Starting writeDoc for $libname...\n";

	$debug = $main::debuggen;

	mkpath( $outputdir ) unless -f $outputdir;

	# Preparse everything, to prepare some additional data in the classes and methods
	Iter::LocalCompounds( $rootnode, sub { preParseClass( shift ); } );

	print STDERR "Writing generateddata.cc...\n";

	writeInheritanceFile($rootnode);

	print STDERR "Done.\n";
}

=head2 preParseClass
	Called for each class
=cut
sub preParseClass
{
	my( $classNode ) = @_;
	my $className = join( "::", kdocAstUtil::heritage($classNode) );

	if ( $className =~ /Proto$/ ) {
	    my $c = $className;
	    $c =~ s/Proto$//;
	    #print STDERR "$c -> $className\n";
		
	    $hasFunctions{$c} = $className; # Associate class -> proto
	    print STDERR "Found proto $className -> skipping\n";
	    $skippedClasses{$className} = 1; # Skip proto
	    return;
	} elsif ( $className =~ /Helper/ ) {
	    print STDERR "Found internal $className -> skipping\n";
	    $skippedClasses{$className} = 1; # Skip internals
	} elsif ( $className =~ /Constants/ ) {
	    print STDERR "Found constants $className -> skipping\n";
	    $skippedClasses{$className} = 1; # Skip internals
	}

	if( $classNode->{Access} eq "private" ||
	    $classNode->{Access} eq "protected" || # e.g. QPixmap::QPixmapData
	    exists $classNode->{Tmpl} ||
	    $className eq 'KJS' || $className eq 'KDOM' || # namespaces
	    $classNode->{NodeType} eq 'union' # Skip unions for now, e.g. QPDevCmdParam
	  ) {
	    print STDERR "Skipping $className "; #if ($debug);

	    #print STDERR "(nothing in it)\n" if ( $#{$classNode->{Kids}} < 0 );
	    if ( exists $classNode->{Tmpl} ) {
		print STDERR "(template)\n";
	    } elsif ( $classNode->{Access} eq "private" or $classNode->{Access} eq "protected" ) {
	        print STDERR "(not public)\n";
	    } elsif ( $classNode->{NodeType} eq 'union') {
		print STDERR "(union)\n";
	    } else {
		print STDERR "\n";
	    }
	    $skippedClasses{$className} = 1;
	    return;
	}

    # Iterate over methods
	
    Iter::MembersByType ( $classNode, undef,
			  sub {	my ($classNode, $methodNode ) = @_;
          if ( $methodNode->{NodeType} eq 'method' ) {
        if ( $methodNode->{astNodeName} eq 'get' ) {
		$hasBridge{$className} = '1';
        $hasGet{$className} = '1';
        } elsif ( $methodNode->{astNodeName} eq 'getforward' ) {
        $hasGet{$className} = '2';
        } elsif ( $methodNode->{astNodeName} eq 'put' ) {
        $hasPut{$className} = '1';
        } elsif ( $methodNode->{astNodeName} eq 'putforward' ) {
        $hasPut{$className} = '2';
        } elsif ( $methodNode->{astNodeName} eq 'getValueProperty' ) {
        $hasHashTable{$className} = '1';
        } elsif ( $methodNode->{astNodeName} eq 'cast' ) {
		$hasCast{$className} = '1';
		}
	}
			    } );
}

# List of all super-classes for a given class
sub superclass_list($)
{
    my $classNode = shift;
    my @super;
    Iter::Ancestors( $classNode, $rootnode, undef, undef, sub {
			push @super, @_[0];
			push @super, superclass_list( @_[0] );
		     }, undef );
    return @super;
}

# Adds the header for node $1 to be included in $2 if not already there
# Prints out debug stuff if $3
sub addIncludeForClass($$$)
{
    my ( $node, $addInclude, $debugMe ) = @_;
    my $sourcename = $node->{Source}->{astNodeName};
    $sourcename =~ s!.*/(.*)!$1!m;
    unless ( defined $addInclude->{$sourcename} ) {
	print "  Including $sourcename\n" if ($debugMe);
	$addInclude->{$sourcename} = 1;
    }
    else { print "  $sourcename already included.\n" if ($debugMe); }
}

=head2
	Write out the smokedata.cpp file containing all the arrays.
=cut

sub writeInheritanceFile($) {
    my $rootnode = shift;

    # Make list of classes
    my %allIncludes; # list of all header files for all classes
    my @classlist;
    push @classlist, ""; # Prepend empty item for "no class"
    Iter::LocalCompounds( $rootnode, sub {
	my $classNode = $_[0];
	my $className = join( "::", kdocAstUtil::heritage($classNode) );
        return if ( defined $skippedClasses{$className} );
	push @classlist, $className;
	$classNode->{ClassIndex} = $#classlist;
	addIncludeForClass( $classNode, \%allIncludes, undef );
    } );

    my %classidx = do { my $i = 0; map { $_ => $i++ } @classlist };
    #foreach my $debugci (keys %classidx) {
    # print STDERR "$debugci: $classidx{$debugci}\n";
    #}

	my $interfaceData = ""; # collects EcmaInterface lines...
	my $forwardsData = ""; # collects class forwards for the EcmaInterface...

    my $file = "$outputdir/generateddata.cc";
    open OUT, ">$file" or die "Couldn't create $file\n";
    print OUT "#include <kdom/ecma/Ecma.h>\n";
    print OUT "#include <kdom/ecma/EcmaInterface.h>\n";
    print OUT "#include <kdom/ecma/DOMLookup.h>\n";

    foreach my $incl (keys %allIncludes) {
	die if $incl eq '';
	print OUT "#include <$incl>\n";
    }	

    print OUT "\n";

    # Prepare descendants information for each class
    my %descendants; # classname -> list of descendant nodes
    #my $SVGElementImplNode;
    Iter::LocalCompounds( $rootnode, sub {
	my $classNode = shift;
	my $className = join( "::", kdocAstUtil::heritage($classNode) );
	# Get _all_ superclasses (up any number of levels)
	# and store that $classNode is a descendant of $s
	my @super = superclass_list($classNode);
	for my $s (@super) {
	    my $superClassName = join( "::", kdocAstUtil::heritage($s) );
	    Ast::AddPropList( \%descendants, $superClassName, $classNode );
	}
 } );

    # Propagate $hasPut and $hasGet 
    Iter::LocalCompounds( $rootnode, sub {
	my $classNode = shift;
	my $className = join( "::", kdocAstUtil::heritage($classNode) );
	if ( defined $descendants{$className} ) {
	    my @desc = @{$descendants{$className}};
	    for my $d (@desc) {
		my $c = join( "::", kdocAstUtil::heritage($d) );
		$hasPut{$c} = '2' if (!$hasPut{$c} && $hasPut{$className});
		$hasGet{$c} = '2' if (!$hasGet{$c} && $hasGet{$className});
	    }
	}
        # This code prints out the base classes - useful for KDOM_BASECLASS_GET
	if ( 0 && defined $descendants{$className} ) {
	    my $baseClass = 1;
	    Iter::Ancestors( $classNode, $rootnode, sub { # called if no ancestors
			     }, undef, sub { # called for each ancestor
			     my $superClassName = join( "::", kdocAstUtil::heritage($_[0]) );
			     $baseClass = 0 if ( $superClassName ne '' ); # happens with unknown parents;
			 } );
           print STDERR "$className is a base class.\n" if ($baseClass);
	}

   } );

	# Write namespaces
	print OUT "using namespace KJS;\n";
	print OUT "using namespace KDOM;\n\n";
	
    # Write classInfos
    print OUT "// For all classes with generated data: the ClassInfo\n";
    Iter::LocalCompounds( $rootnode, sub {
	my $classNode = shift;
	my $className = join( "::", kdocAstUtil::heritage($classNode) );

	# We use namespace declarations!
	my $printName = $className;
	$printName =~ s/KDOM:://;

	# Skip marked classes
	if ($hasGet{$className} ne '1' && !$skippedClasses{$className}) {
	    $skippedClasses{$className} = '1' ;
	    print STDERR "Skipping $className, no get()\n";
	}

        return if ( defined $skippedClasses{$className} );

	my $ok = $hasHashTable{$className};
	print STDERR "$className has get() but no hashtable - TODO\n" if (!$ok && $hasGet{$className} eq '1');

	print OUT "const ClassInfo ${printName}::s_classInfo = {\"$className\",0,";
	if ($ok) {
	    print OUT "\&${printName}::s_hashTable";
	} else {
	    print OUT "0";
	}
        print OUT ",0};\n";
	#die "problem with $className" unless defined $classinherit{$className};
	#print OUT "const short int ${className}::s_inheritanceIndex = $classinherit{$className};\n";
    } );

    # Generated methods
    print OUT "\n";
	
    Iter::LocalCompounds( $rootnode, sub {
	my $classNode = shift;
	my $className = join( "::", kdocAstUtil::heritage($classNode) );
        return if ( defined $skippedClasses{$className} );

	# We use namespace declarations!
	my $printName = $className;
	$printName =~ s/KDOM:://;

	my $paramsUsed = 0;
	
	print OUT "bool ${printName}::hasProperty(ExecState *p1,const Identifier &p2) const\n";
	print OUT "{\n";

	if ($hasHashTable{$className}) {
	    print OUT "    const HashEntry *e = Lookup::findEntry(\&${printName}::s_hashTable,p2);\n";
	    print OUT "    if(e) return true;\n";
	    $paramsUsed=1;
	}
	# Now look in prototype, if it exists
	if ( defined $hasFunctions{$className} ) {

		# We use namespace declarations!
		my $output = $hasFunctions{$className};
		$output =~ s/KDOM:://;

	    print OUT "    Object proto = " . $output . "::self(p1);\n";
	    print OUT "    if(proto.hasProperty(p1,p2)) return true;\n";
	}
	# For each direct ancestor...
	Iter::Ancestors( $classNode, $rootnode, undef, undef, sub {
			     my $superClassName = join( "::", kdocAstUtil::heritage($_[0]) );
				
				 # We use namespace declarations!
				 my $printSuperClassName = $superClassName;
				 $printSuperClassName =~ s/KDOM:://;

			     if ( $superClassName ne '' ) { # happens with unknown parents
			       return if ( defined $skippedClasses{$superClassName} );
			        print OUT "    if(${printSuperClassName}::hasProperty(p1,p2)) return true;\n";
				$paramsUsed=2;
			     }
		    }, undef );
	if ($paramsUsed == 1 && !defined $hasFunctions{$className}){
	    print OUT "    Q_UNUSED(p1);\n";
	}
	if ($paramsUsed == 0){
	    print OUT "    Q_UNUSED(p1); Q_UNUSED(p2);\n";
	}
	# These are special cases to support [] indexing on them
	if( ${printName} eq 'NodeList' || ${printName} eq 'NamedNodeMap' || ${printName} eq 'MediaList' || ${printName} eq 'StyleSheetList' || ${printName} eq 'CSSRuleList' || ${printName} eq 'CSSStyleDeclaration' || ${printName} eq 'CSSValueList' )
	{
	    print OUT "    bool ok;\n";
	    print OUT "    unsigned int i = p2.toArrayIndex(&ok);\n";
	    print OUT "    if(ok && i < length()) return true;\n";
	}
        print OUT "    return false;\n";
        print OUT "}\n\n";

	my @ancestorsWithGet;
	Iter::Ancestors( $classNode, $rootnode, undef, undef, sub {
			     my $superClassName = join( "::", kdocAstUtil::heritage($_[0]) );
			     if ( $superClassName ne '' # happens with unknown parents
				  && ! defined $skippedClasses{$superClassName} ) {
				 if ( $hasGet{$superClassName} ) {
				     push @ancestorsWithGet, $superClassName;
				 }
			     }
		    }, undef );

	if ($hasHashTable{$className}) {
	    die unless $hasGet{$className};
	    if ( $hasGet{$className} eq '1' ) {
		print OUT "Value ${printName}::get(GET_METHOD_ARGS) const\n";
		print OUT "{\n";
		# These are special cases to support [] indexing on them
		if( ${printName} eq 'NodeList' || ${printName} eq 'NamedNodeMap' || ${printName} eq 'MediaList' ||
			${printName} eq 'StyleSheetList' || ${printName} eq 'CSSRuleList' || ${printName} eq 'CSSStyleDeclaration' ||
			${printName} eq 'CSSValueList' ) {
			print OUT "    bool ok;\n";
			print OUT "    unsigned int i = p2.toArrayIndex(&ok);\n";
			
			if( ${printName} eq 'CSSStyleDeclaration' || ${printName} eq 'MediaList' ) {
				print OUT "    if(ok && i < length()) return getDOMString(item(i));\n";
			} else {
				if( ${printName} eq 'NodeList') {
					print OUT "    if(ok && i < length()) return getDOMNode(p1, item(i));\n";
				} else {
					print OUT "    if(ok && i < length()) return item(i).cache(p1);\n";
				}
			}
		}
		
		if ( defined $hasFunctions{$className} ) {

			# We use namespace declarations!
			my $output = $hasFunctions{$className};
			$output =~ s/KDOM:://;

		    print OUT "    return lookupGet<${output}Func,${printName}>(p1,p2,&s_hashTable,this,p3);\n";
		} else {
		    print OUT "    return lookupGetValue<${printName}>(p1,p2,&s_hashTable,this,p3);\n";
		}
		print OUT "}\n\n";

		if ( defined ($hasFunctions{$className}) ) {	
			my $output = $hasFunctions{$className};
			$output =~ s/KDOM:://;

		    my $methodNameOrig = "${output}Func::cast";

			$output =~ s/Proto//;
			ucfirst($output);
			
			my $methodName = "to${output}";

			if ( $hasCast{$className} eq '1' ) {
				$methodName = "KDOM::" . $methodName;

				print OUT "${printName} ${output}ProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const\n";
				print OUT "{\n";
				print OUT "    return $methodName(exec, p1);\n";
				print OUT "}\n\n";
			} else {
				$methodName = $methodNameOrig;
			}

		    # Type resolver for the Func class
			if($methodName eq $methodNameOrig) {
			    print OUT "${printName} $methodName(KJS::ExecState *exec, const ObjectImp *p1) const\n";
			} else {
			    print OUT "${printName} $methodName(KJS::ExecState *exec, const ObjectImp *p1)\n";
			}

		    print OUT "{\n";
		    
			my @toTry;		    
			push @toTry, $classNode;
		    if ( defined $descendants{$className} ) {
				push @toTry, @{$descendants{$className}};
		    }
			
		    foreach my $d (@toTry) {
				my $c = join( "::", kdocAstUtil::heritage($d) );

				# We use namespace declarations!
				my $d = $c;
				$d =~ s/KDOM:://;
			
				print OUT "    { const DOMBridge<$d> *test = dynamic_cast<const DOMBridge<$d> * >(p1);\n";
				print OUT "    if(test) return ${printName}(static_cast<${printName}::Private *>(test->impl())); }\n";
		    }

			print OUT "    if(!exec) return ${output}::null;\n";
		    print OUT "    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());\n";
			print OUT "    DocumentImpl *document = (interpreter ? interpreter->document() : 0);\n";
			print OUT "    Ecma *engine = (document ? document->ecmaEngine() : 0);\n";
			print OUT "    EcmaInterface *interface = (engine ? engine->interface() : 0);\n";
			print OUT "    if(!interface) return ${output}::null;\n";
		    print OUT "    return interface->inherited${output}Cast(p1);\n";
		    print OUT "}\n\n";

			# Implement inheritedClassCast...
			$forwardsData = $forwardsData . "\tclass ${output};\n";
			$interfaceData = $interfaceData . "\t\tvirtual ${output} inherited${output}Cast(const KJS::ObjectImp *p1);\n";
			print OUT "${output} KDOM::EcmaInterface::inherited${output}Cast(const ObjectImp *p1)\n";
			print OUT "{\n\tQ_UNUSED(p1); return ${output}::null;\n}\n\n";
		} elsif($hasCast{$className} eq '1' ) {
			my $output = $className;
			$output =~ s/KDOM:://;
			$output =~ s/Proto//;
			ucfirst($output);
			
			my $methodName = "to${output}";

		    # Type resolver for the Func class
		    print OUT "${printName} KDOM::$methodName(KJS::ExecState *exec, const ObjectImp *p1)\n";
	    	print OUT "{\n";
			print OUT "\tQ_UNUSED(exec);\n";
		    my @toTry;
		    push @toTry, $classNode;
		    if ( defined $descendants{$className} ) {
			push @toTry, @{$descendants{$className}};
	    	}
		    foreach my $d (@toTry) {
			my $c = join( "::", kdocAstUtil::heritage($d) );

			# We use namespace declarations!
			my $d = $c;
			$d =~ s/KDOM:://;
			
			print OUT "    { const DOMBridge<$d> *test = dynamic_cast<const DOMBridge<$d> * >(p1);\n";
			print OUT "    if(test) return ${printName}(test->impl()); }\n";
			}
			print OUT "    return ${printName}::null;\n";
			print OUT "}\n\n";
		}
		}
	}
	my $methodName = $hasGet{$className} eq '1' ? 'getInParents' : 'get';
        print OUT "Value ${printName}::$methodName(GET_METHOD_ARGS) const\n";
        print OUT "{\n";
	my $paramsUsed = 0;
	# Now look in prototype, if it exists
	if ( defined $hasFunctions{$className} ) {
	    # Prototype exists (because the class has functions)

		# We use namespace declarations!
		my $output = $hasFunctions{$className};
		$output =~ s/KDOM:://;

	    print OUT "    Object proto = " . $output . "::self(p1);\n";
	    print OUT "    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);\n"; ## TODO get() directly
	    $paramsUsed = 1;
	}
	foreach my $anc (@ancestorsWithGet) {
		# We use namespace declarations!
		my $printAnc = $anc;
		$printAnc =~ s/KDOM:://;

		print OUT "    if(${printAnc}::hasProperty(p1,p2)) return ${printAnc}::get(p1,p2,p3);\n"; ## TODO get() directly
	    $paramsUsed = 2;
	}

	if ($paramsUsed == 0 ){
	    print OUT "    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);\n";
	} elsif ( $paramsUsed == 1 ) {
	    print OUT "    Q_UNUSED(p3);\n";
	}
        print OUT "    return Undefined();\n";
        print OUT "}\n\n";

	if ( $hasPut{$className} )
	{
	    if ( $hasPut{$className} eq '1' ) {
		if ($hasHashTable{$className}) {
		    print OUT "bool ${printName}::put(PUT_METHOD_ARGS)\n";
		    print OUT "{\n";
		    print OUT "    return lookupPut<${printName}>(p1,p2,p3,p4,&s_hashTable,this);\n";
		    print OUT "}\n\n";
		}
		print OUT "bool ${printName}::putInParents(PUT_METHOD_ARGS)\n";
	    } else { # forward put
		print OUT "bool ${printName}::put(PUT_METHOD_ARGS)\n";
	    }
	    print OUT "{\n";
	    my $paramsUsed = 0;
	    Iter::Ancestors( $classNode, $rootnode, undef, undef, sub {
			     my $superClassName = join( "::", kdocAstUtil::heritage($_[0]) );

				 # We use namespace declarations!
				 my $printSuperClassName = $superClassName;
				 $printSuperClassName =~ s/KDOM:://;

			     if ( $superClassName ne '' ) { # happens with unknown parents
				 if ( $hasPut{$superClassName} ) {
				     print OUT "    if(${printSuperClassName}::hasProperty(p1,p2)) {\n";
				     print OUT "        ${printSuperClassName}::put(p1,p2,p3,p4);\n";
				     print OUT "        return true;\n";
				     print OUT "    }\n";
				     $paramsUsed=1;
				 }
			     }
		    }, undef );
	    if (!$paramsUsed){
		print OUT "    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);\n";
	    }
	    print OUT "    return false;\n";
	    print OUT "}\n\n";
	}

	# Write prototype method
	print OUT "Object ${printName}::prototype(ExecState *p1) const\n";
	print OUT "{\n";
	if ( defined $hasFunctions{$className} ) {

		# We use namespace declarations!
		my $output = $hasFunctions{$className};
		$output =~ s/KDOM:://;

	    # Prototype exists (because the class has functions)
	    print OUT "    if(p1) return " . $output . "::self(p1);\n";
	} else {
	    # Standard Object prototype
	    print OUT "    if(p1) return p1->interpreter()->builtinObjectPrototype();\n";
	}
	print OUT "    return Object::dynamicCast(Null());\n"; # hmm

        print OUT "}\n\n";

	# Process classes only with KDOM_BRIDGE
	if ($hasBridge{$className}) {

		#print STDERR "Writing bridge() for $className...\n";
		
		# Write bridge method
		print OUT "ObjectImp *${printName}::bridge(ExecState *p1) const\n";
		print OUT "{\n";
		
		my $handleName = "d";
		if ($className eq "KDOM::Document") {
			$handleName = "KDOM::EventTarget::d";
		}		
		
		if ($hasPut{$className}) {
			print OUT "    return new DOMRWBridge<${printName}>(p1,static_cast<${printName}::Private *>(${handleName}));\n";
		}
		else {
			print OUT "    return new DOMBridge<${printName}>(p1,static_cast<${printName}::Private *>(${handleName}));\n";
		}
		
		print OUT "}\n\n";
	}

	if ($hasGet{$className}) {
		my $handleName = "d";
		if ($className eq "KDOM::Document") {
			$handleName = "KDOM::EventTarget::d";
		}		

		# Write cache method
		print OUT "Value ${printName}::cache(ExecState *p1) const\n";
		print OUT "{\n";
		print OUT "    return cacheDOMObject<${printName}>(p1,static_cast<${printName}::Private *>(${handleName}));\n";
		print OUT "}\n\n";
	}	
   } );

	my $interfaceFile = "$outputdir/EcmaInterface.h";
    open OUTINT, ">$interfaceFile" or die "Couldn't create $interfaceFile\n";

	print OUTINT <<'    EOF';
/*
    Copyright (C) 2004-2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004-2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project
	This file is autogenerated using 'make generatedata'!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_EcmaInterface_H
#define KDOM_EcmaInterface_H

namespace KJS
{
	class ObjectImp;
};

namespace KDOM
{
    EOF
	
	print OUTINT $forwardsData . "\n";
	print OUTINT "\tclass EcmaInterface\n\t{\n\tpublic:\n\t\tEcmaInterface() { }\n\t\tvirtual ~EcmaInterface() { }\n\n";
	print OUTINT $interfaceData;
	print OUTINT "\t};\n};\n\n#endif\n";
}
1;
