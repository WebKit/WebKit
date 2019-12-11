<?php

# TODO
# Implement correct number formats (don't forget imaginaries)
# Implement correct string/bytes formats (escape sequences)
# Add type “keywords”?
# <http://docs.python.org/dev/3.0/reference/lexical_analysis.html>

class PythonLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'Python',
            parent::VERSION => '0.1',
            parent::AUTHOR => array(
                parent::NAME => 'Konrad Rudolph',
                parent::WEBSITE => 'madrat.net',
                parent::EMAIL => 'konrad_rudolph@madrat.net'
            )
        ));

        $this->setExtensions(array('py'));

        $this->setCaseInsensitive(false);

        $this->addStates(array(
            'init' => array(
                'string',
                'bytes',
                'number',
                'comment',
                'keyword' => array('', 'literal', 'operator'),
                'identifier'
            ),
        ));

        $this->addRules(array(
            'string' => Rule::C_DOUBLEQUOTESTRING,
            'bytes' => Rule::C_SINGLEQUOTESTRING,
            'number' => Rule::C_NUMBER,
            'comment' => '/#.*/',
            'keyword' => array(
                array(
                    'assert', 'break', 'class', 'continue', 'def', 'del',
                    'elif', 'else', 'except', 'finally', 'for', 'from',
                    'global', 'if', 'import', 'in', 'lambda', 'nonlocal',
                    'pass', 'raise', 'return', 'try', 'while', 'with', 'yield'
                ),
                'literal' => array(
                    'False', 'None', 'True'
                ),
                'operator' => array(
                    'and', 'as', 'is', 'not', 'or'
                )
            ),
            'identifier' => Rule::C_IDENTIFIER,
        ));

        $this->addMappings(array(
            'bytes' => 'char'
        ));
    }
}

?>
