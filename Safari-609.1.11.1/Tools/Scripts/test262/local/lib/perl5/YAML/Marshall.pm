use strict; use warnings;
package YAML::Marshall;

use YAML::Node ();

sub import {
    my $class = shift;
    no strict 'refs';
    my $package = caller;
    unless (grep { $_ eq $class} @{$package . '::ISA'}) {
        push @{$package . '::ISA'}, $class;
    }

    my $tag = shift;
    if ( $tag ) {
        no warnings 'once';
        $YAML::TagClass->{$tag} = $package;
        ${$package . "::YamlTag"} = $tag;
    }
}

sub yaml_dump {
    my $self = shift;
    no strict 'refs';
    my $tag = ${ref($self) . "::YamlTag"} || 'perl/' . ref($self);
    $self->yaml_node($self, $tag);
}

sub yaml_load {
    my ($class, $node) = @_;
    if (my $ynode = $class->yaml_ynode($node)) {
        $node = $ynode->{NODE};
    }
    bless $node, $class;
}

sub yaml_node {
    shift;
    YAML::Node->new(@_);
}

sub yaml_ynode {
    shift;
    YAML::Node::ynode(@_);
}

1;
