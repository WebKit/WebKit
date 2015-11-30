<?php

class CsharpLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'C#',
            parent::VERSION => '0.3',
            parent::AUTHOR => array(
                parent::NAME => 'Konrad Rudolph',
                parent::WEBSITE => 'madrat.net',
                parent::EMAIL => 'konrad_rudolph@madrat.net'
            )
        ));

        $this->setExtensions(array('cs'));

        $this->setCaseInsensitive(false);

        $this->addStates(array(
            'init' => array(
                'string',
                'char',
                'number',
                'comment' => array('', 'doc'),
                'keyword' => array('', 'type', 'literal', 'operator', 'preprocessor'),
                'identifier',
                'operator',
                'whitespace',
            ),
            'comment doc' => 'doc',
        ));

        $this->addRules(array(
            'whitespace' => Rule::ALL_WHITESPACE,
            'operator' => '/[-+*\/%&|^!~=<>?{}()\[\].,:;]|&&|\|\||<<|>>|[-=!<>+*\/%&|^]=|<<=|>>=|->/',
            'string' => Rule::C_DOUBLEQUOTESTRING,
            'char' => Rule::C_SINGLEQUOTESTRING,
            'number' => Rule::C_NUMBER,
            'comment' => array(
                '#//(?:[^/].*?)?\n|/\*.*?\*/#s',
                'doc' => new Rule('#///#', '/$/m')
            ),
            'doc' => '/<(?:".*?"|\'.*?\'|[^>])*>/',
            'keyword' => array(
                array(
                    'abstract', 'break', 'case', 'catch', 'checked', 'class',
                    'const', 'continue', 'default', 'delegate', 'do', 'else',
                    'enum', 'event', 'explicit', 'extern', 'finally', 'fixed',
                    'for', 'foreach', 'goto', 'if', 'implicit', 'in', 'interface',
                    'internal', 'lock', 'namespace', 'operator', 'out', 'override',
                    'params', 'private', 'protected', 'public', 'readonly', 'ref',
                    'return', 'sealed', 'static', 'struct', 'switch', 'throw',
                    'try', 'unchecked', 'unsafe', 'using', 'var', 'virtual',
                    'volatile', 'while'
                ),
                'type' => array(
                    'bool', 'byte', 'char', 'decimal', 'double', 'float', 'int',
                    'long', 'object', 'sbyte', 'short', 'string', 'uint', 'ulong',
                    'ushort', 'void'
                ),
                'literal' => array(
                    'base', 'false', 'null', 'this', 'true',
                ),
                'operator' => array(
                    'as', 'is', 'new', 'sizeof', 'stackallock', 'typeof',
                ),
                'preprocessor' => '/#(?:if|else|elif|endif|define|undef|warning|error|line|region|endregion)/'
            ),
            'identifier' => '/@?[a-z_][a-z0-9_]*/i',
        ));

        $this->addMappings(array(
            'whitespace' => '',
            'operator' => '',
        ));
    }
}

?>
