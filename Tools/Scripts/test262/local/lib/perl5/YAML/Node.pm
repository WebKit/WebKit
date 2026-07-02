use strict; use warnings;
package YAML::Node;

use YAML::Tag;
require YAML::Mo;

use Exporter;
our @ISA     = qw(Exporter YAML::Mo::Object);
our @EXPORT  = qw(ynode);

sub ynode {
    my $self;
    if (ref($_[0]) eq 'HASH') {
        $self = tied(%{$_[0]});
    }
    elsif (ref($_[0]) eq 'ARRAY') {
        $self = tied(@{$_[0]});
    }
    elsif (ref(\$_[0]) eq 'GLOB') {
        $self = tied(*{$_[0]});
    }
    else {
        $self = tied($_[0]);
    }
    return (ref($self) =~ /^yaml_/) ? $self : undef;
}

sub new {
    my ($class, $node, $tag) = @_;
    my $self;
    $self->{NODE} = $node;
    my (undef, $type) = YAML::Mo::Object->node_info($node);
    $self->{KIND} = (not defined $type) ? 'scalar' :
                    ($type eq 'ARRAY') ? 'sequence' :
                    ($type eq 'HASH') ? 'mapping' :
                    $class->die("Can't create YAML::Node from '$type'");
    tag($self, ($tag || ''));
    if ($self->{KIND} eq 'scalar') {
        yaml_scalar->new($self, $_[1]);
        return \ $_[1];
    }
    my $package = "yaml_" . $self->{KIND};
    $package->new($self)
}

sub node { $_->{NODE} }
sub kind { $_->{KIND} }
sub tag {
    my ($self, $value) = @_;
    if (defined $value) {
               $self->{TAG} = YAML::Tag->new($value);
        return $self;
    }
    else {
       return $self->{TAG};
    }
}
sub keys {
    my ($self, $value) = @_;
    if (defined $value) {
               $self->{KEYS} = $value;
        return $self;
    }
    else {
       return $self->{KEYS};
    }
}

#==============================================================================
package yaml_scalar;

@yaml_scalar::ISA = qw(YAML::Node);

sub new {
    my ($class, $self) = @_;
    tie $_[2], $class, $self;
}

sub TIESCALAR {
    my ($class, $self) = @_;
    bless $self, $class;
    $self
}

sub FETCH {
    my ($self) = @_;
    $self->{NODE}
}

sub STORE {
    my ($self, $value) = @_;
    $self->{NODE} = $value
}

#==============================================================================
package yaml_sequence;

@yaml_sequence::ISA = qw(YAML::Node);

sub new {
    my ($class, $self) = @_;
    my $new;
    tie @$new, $class, $self;
    $new
}

sub TIEARRAY {
    my ($class, $self) = @_;
    bless $self, $class
}

sub FETCHSIZE {
    my ($self) = @_;
    scalar @{$self->{NODE}};
}

sub FETCH {
    my ($self, $index) = @_;
    $self->{NODE}[$index]
}

sub STORE {
    my ($self, $index, $value) = @_;
    $self->{NODE}[$index] = $value
}

sub undone {
    die "Not implemented yet"; # XXX
}

*STORESIZE = *POP = *PUSH = *SHIFT = *UNSHIFT = *SPLICE = *DELETE = *EXISTS =
*STORESIZE = *POP = *PUSH = *SHIFT = *UNSHIFT = *SPLICE = *DELETE = *EXISTS =
*undone; # XXX Must implement before release

#==============================================================================
package yaml_mapping;

@yaml_mapping::ISA = qw(YAML::Node);

sub new {
    my ($class, $self) = @_;
    @{$self->{KEYS}} = sort keys %{$self->{NODE}};
    my $new;
    tie %$new, $class, $self;
    $new
}

sub TIEHASH {
    my ($class, $self) = @_;
    bless $self, $class
}

sub FETCH {
    my ($self, $key) = @_;
    if (exists $self->{NODE}{$key}) {
        return (grep {$_ eq $key} @{$self->{KEYS}})
               ? $self->{NODE}{$key} : undef;
    }
    return $self->{HASH}{$key};
}

sub STORE {
    my ($self, $key, $value) = @_;
    if (exists $self->{NODE}{$key}) {
        $self->{NODE}{$key} = $value;
    }
    elsif (exists $self->{HASH}{$key}) {
        $self->{HASH}{$key} = $value;
    }
    else {
        if (not grep {$_ eq $key} @{$self->{KEYS}}) {
            push(@{$self->{KEYS}}, $key);
        }
        $self->{HASH}{$key} = $value;
    }
    $value
}

sub DELETE {
    my ($self, $key) = @_;
    my $return;
    if (exists $self->{NODE}{$key}) {
        $return = $self->{NODE}{$key};
    }
    elsif (exists $self->{HASH}{$key}) {
        $return = delete $self->{NODE}{$key};
    }
    for (my $i = 0; $i < @{$self->{KEYS}}; $i++) {
        if ($self->{KEYS}[$i] eq $key) {
            splice(@{$self->{KEYS}}, $i, 1);
        }
    }
    return $return;
}

sub CLEAR {
    my ($self) = @_;
    @{$self->{KEYS}} = ();
    %{$self->{HASH}} = ();
}

sub FIRSTKEY {
    my ($self) = @_;
    $self->{ITER} = 0;
    $self->{KEYS}[0]
}

sub NEXTKEY {
    my ($self) = @_;
    $self->{KEYS}[++$self->{ITER}]
}

sub EXISTS {
    my ($self, $key) = @_;
    exists $self->{NODE}{$key}
}

1;
