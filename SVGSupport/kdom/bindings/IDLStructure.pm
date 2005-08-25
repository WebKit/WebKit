#
# KDOM IDL parser
#
# Copyright (c) 2005 Nikolas Zimmermann <wildfox@kde.org>
#
package IDLStructure;

use Class::Struct;

# Used to represent a parsed IDL document
struct( idlDocument => {
		module => '$',	 # Module identifier
		classes => '@'	 # All parsed interfaces.
});

# Used to represent 'interface' / 'exception' blocks
struct( domClass => {
		name => '$',		# Class identifier (without module)
		parents => '@',		# List of strings
		constants => '@',	# List of 'domConstant'
		functions => '@',	# List of 'domFunction'
		attributes => '@',	# List of 'domAttribute'
		documentation => '$',

		# Special cases here!
		'noDPtrFlag' => '$'	# Forces the code generator to not generate
							# a d-ptr for this base class (only use with such!)
});

# Used to represent domClass contents (name of method, signature)
struct( domFunction => {
		signature => '$',	# Return type/Object name/PtrFlag
		parameters => '@',	# List of 'domSignature'
		documentation => '$'
});

# Used to represent a map of 'variable name' <-> 'variable type'
struct( domSignature => {
		name => '$',		# Variable name
		type => '$',		# Variable type
		hasPtrFlag => '$',	# Ptr (*) flag?
		documentation => '$'
});

# Used to represent domClass contents (name of method, signature)
struct( domAttribute => {
		type => '$',				# Attribute type (including namespace)
		signature => '$',			# Attribute signature
		exceptionName => '$',		# Name of exception
		exceptionDocumentation => '@', # Documentation per exception
		documentation => '@'
});

# Used to represent string constants
struct( domConstant => {
		name => '$',	# DOM Constant identifier
		type => '$',	# Type of data
		value => '$',	# Constant value
		documentation => '$'
});

# Helpers
$idlId = '[a-zA-Z0-9]';			# Generic identifier
$idlIdNs = '[a-zA-Z0-9:]';		# Generic identifier including namespace
$idlType = '[a-zA-Z0-9_]';		# Generic type/"value string" identifier
$idlDataType = '[a-zA-Z0-9\ ]'; # Generic data type identifier

# Magic IDL parsing regular expressions
my $supportedTypes = "((?:unsigned )?(?:int|short|long)|(?:$idlIdNs*))";

# Special IDL notations
$ptrSyntax = '\[ptr\]'; # ie. 'attribute [ptr] DOMString foo;"
$nodptrSyntax = '\[nodptr\]'; # ie. 'interface [nodptr] Foobar { ..."

# Regular expression based IDL 'syntactical tokenizer' used in the IDLParser
$moduleSelector = 'module\s*(' . $idlId . '*)\s*{';
$moduleNSSelector = 'module\s*(' . $idlId . '*)\s*\[ns\s*(' . $idlIdNs . '*)\s*\]\s*;';
$constantSelector = 'const\s*' . $supportedTypes . '\s*(' . $idlType . '*)\s*=\s*(' . $idlType . '*)';

$typeNamespaceSelector = '((?:' . $idlId . '*::)*)\s*(' . $idlDataType . '*)';

$exceptionSelector = 'exception\s*(' . $idlIdNs . '*)\s*([a-zA-Z\s{;]*};)';
$exceptionSubSelector = '{\s*' . $supportedTypes . '\s*(' . $idlType . '*)\s*;\s*}';

$interfaceSelector = 'interface\s*((?:' . $nodptrSyntax . ' )?)(' . $idlIdNs . '*)\s*(?::(\s*[^{]*))?{([a-zA-Z0-9_=\s(),;:\[\]]*)';
$interfaceMethodSelector = '\s*((?:' . $ptrSyntax . ' )?)' . $supportedTypes . '\s*(' . $idlIdNs . '*)\s*\(\s*([a-zA-Z0-9:\s,\[\]]*)';
$interfaceParameterSelector = 'in\s*((?:' . $ptrSyntax . ' )?)' . $supportedTypes . '\s*(' . $idlIdNs . '*)';

$interfaceExceptionSelector = 'raises\s*\((' . $idlIdNs . '*)\s*\)';
$interfaceAttributeSelector = '\s*(readonly attribute|attribute)\s*(' . $ptrSyntax . ' )?' . $supportedTypes . '\s*(' . $idlType . '*)';

1;
