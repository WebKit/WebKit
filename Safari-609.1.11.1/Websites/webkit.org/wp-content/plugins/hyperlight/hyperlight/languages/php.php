<?php

// TODO
// - Fill the scaffold below!
// - Comments via #
// - More keywords? What about functions?
// - String interpolation and escaping.
// - Usual stuff for doc comments
// - Heredoc et al.
// - More complex nested variable names.

class PhpLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'PHP',
            parent::VERSION => '0.3',
            parent::AUTHOR => array(
                parent::NAME => 'Konrad Rudolph',
                parent::WEBSITE => 'madrat.net',
                parent::EMAIL => 'konrad_rudolph@madrat.net'
            )
        ));

        $this->setExtensions(array('php', 'php3', 'php4', 'php5', 'inc'));

        $this->addPostProcessing('html', HyperLanguage::fromName('xml'));

        $this->addStates(array(
            'init' => array('php', 'html'),
            'php' => array(
                'comment', 'string', 'char', 'number',
                'keyword' => array('', 'type', 'literal', 'operator', 'builtin'),
                'identifier', 'variable'),
            'variable' => array('identifier'),
            'html' => array()
        ));

        $this->addRules(array(
            'php' => new Rule('/<\?php/', '/\?>/'),
            'html' => new Rule('/(?=.)/', '/(?=<\?php)/'),
            'comment' => Rule::C_COMMENT,
            'string' => Rule::C_DOUBLEQUOTESTRING,
            'char' => Rule::C_SINGLEQUOTESTRING,
            'number' => Rule::C_NUMBER,
            'identifier' => Rule::C_IDENTIFIER,
            'variable' => new Rule('/\$/', '//'),
            'keyword' => array(
                array('break', 'case', 'class', 'const', 'continue', 'declare', 'default', 'do', 'else', 'elseif', 'enddeclare', 'endfor', 'endforeach', 'endif', 'endswitch', 'endwhile', 'extends', 'for', 'foreach', 'function', 'global', 'if', 'return', 'static', 'switch', 'use', 'var', 'while', 'final', 'interface', 'implements', 'public', 'private', 'protected', 'abstract', 'try', 'catch', 'throw', 'final', 'namespace'),
                'type' => array('exception', 'int'),
                'literal' => array('false', 'null', 'true', 'this'),
                'operator' => array('and', 'as', 'or', 'xor', 'new', 'instanceof', 'clone'),
                'builtin' => array('array', 'die', 'echo', 'empty', 'eval', 'exit', 'include', 'include_once', 'isset', 'list', 'print', 'require', 'require_once', 'unset')
            ),
        ));

        $this->addMappings(array(
            'char' => 'string',
            'variable' => 'tag',
            'html' => 'preprocessor',
        ));
    }
}

?>
