<?php

// TODO:
// - Add escaped string characters
// - Add 'TO DO', 'FIX ME', … tags
// - (Add doc comments?)

class CppLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'C++',
            parent::VERSION => '0.4',
            parent::AUTHOR => array(
                parent::NAME => 'Konrad Rudolph',
                parent::WEBSITE => 'madrat.net',
                parent::EMAIL => 'konrad_rudolph@madrat.net'
            )
        ));

        $this->setExtensions(array('c', 'cc', 'cpp', 'h', 'hpp', 'icl', 'ipp'));

        $keyword = array('keyword' => array('', 'type', 'literal', 'operator'));
        $common = array(
            'string', 'char', 'number', 'comment',
            'keyword' => array('', 'type', 'literal', 'operator'),
            'identifier',
            'operator'
        );

        $this->addStates(array(
            'init' => array_merge(array('include', 'preprocessor'), $common),
            'include' => array('incpath'),
            'preprocessor' => array_merge($common, array('pp_newline')),
        ));

        $this->addRules(array(
            'whitespace' => RULE::ALL_WHITESPACE,
            'operator' => '/<:|:>|<%|%>|%:|%:%:|\+\+|--|&&|\|\||::|<<|>>|##|\.\.\.|\.\*|->|->*|[-+*\/%^&|!~<>.=,;:?()\[\]\{\}]|[-+*\/%^&|=!~<>]=|<<=|>>=/',
            'include' => new Rule('/#\s*include/', '/\n/'),
            'preprocessor' => new Rule('/#\s*\w+/', '/\n/'),
            //'pp_newline' => '/[^\\\\](?<bs>\\\\*?)(?P=bs)\\\\\n/',
            'pp_newline' => '/(?<!\\\\)(?:\\\\\\\\)*?\\\\\n/',
            'incpath' => '/<[^>]*>|"[^"]*"/',
            'string' => Rule::C_DOUBLEQUOTESTRING,
            'char' => Rule::C_SINGLEQUOTESTRING,
            'number' => Rule::C_NUMBER,
            'comment' => Rule::C_COMMENT,
            'keyword' => array(
                array(
                    'asm', 'auto', 'break', 'case', 'catch', 'class', 'const',
                    'const_cast', 'continue', 'default', 'do', 'dynamic_cast',
                    'else', 'enum', 'explicit', 'export', 'extern', 'for',
                    'firend', 'goto', 'if', 'inline', 'mutable', 'namespace',
                    'operator', 'private', 'protected', 'public', 'register',
                    'reinterpret_cast', 'return', 'sizeof', 'static',
                    'static_cast', 'struct', 'switch', 'template', 'throw',
                    'try', 'typedef', 'typename', 'union', 'using', 'virtual',
                    'volatile', 'while'
                ),
                'type' => array(
                    'bool', 'char', 'double', 'float', 'int', 'long', 'short',
                    'signed', 'unsigned', 'void', 'wchar_t'
                ),
                'literal' => array(
                    'false', 'this', 'true'
                ),
                'operator' => array(
                    'and', 'and_eq', 'bitand', 'bitor', 'compl', 'delete',
                    'new', 'not', 'not_eq', 'or', 'or_eq', 'typeid', 'xor',
                    'xor_eq'
                ),
            ),
            'identifier' => Rule::C_IDENTIFIER,
        ));

        $this->addMappings(array(
            'operator' => '',
            'include' => 'preprocessor',
            'incpath' => 'tag',
        ));
    }
}

?>
