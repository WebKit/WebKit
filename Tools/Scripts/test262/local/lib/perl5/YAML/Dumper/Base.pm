package YAML::Dumper::Base;

use YAML::Mo;

use YAML::Node;

# YAML Dumping options
has spec_version    => default => sub {'1.0'};
has indent_width    => default => sub {2};
has use_header      => default => sub {1};
has use_version     => default => sub {0};
has sort_keys       => default => sub {1};
has anchor_prefix   => default => sub {''};
has dump_code       => default => sub {0};
has use_block       => default => sub {0};
has use_fold        => default => sub {0};
has compress_series => default => sub {1};
has inline_series   => default => sub {0};
has use_aliases     => default => sub {1};
has purity          => default => sub {0};
has stringify       => default => sub {0};
has quote_numeric_strings => default => sub {0};

# Properties
has stream      => default => sub {''};
has document    => default => sub {0};
has transferred => default => sub {{}};
has id_refcnt   => default => sub {{}};
has id_anchor   => default => sub {{}};
has anchor      => default => sub {1};
has level       => default => sub {0};
has offset      => default => sub {[]};
has headless    => default => sub {0};
has blessed_map => default => sub {{}};

# Global Options are an idea taken from Data::Dumper. Really they are just
# sugar on top of real OO properties. They make the simple Dump/Load API
# easy to configure.
sub set_global_options {
    my $self = shift;
    $self->spec_version($YAML::SpecVersion)
      if defined $YAML::SpecVersion;
    $self->indent_width($YAML::Indent)
      if defined $YAML::Indent;
    $self->use_header($YAML::UseHeader)
      if defined $YAML::UseHeader;
    $self->use_version($YAML::UseVersion)
      if defined $YAML::UseVersion;
    $self->sort_keys($YAML::SortKeys)
      if defined $YAML::SortKeys;
    $self->anchor_prefix($YAML::AnchorPrefix)
      if defined $YAML::AnchorPrefix;
    $self->dump_code($YAML::DumpCode || $YAML::UseCode)
      if defined $YAML::DumpCode or defined $YAML::UseCode;
    $self->use_block($YAML::UseBlock)
      if defined $YAML::UseBlock;
    $self->use_fold($YAML::UseFold)
      if defined $YAML::UseFold;
    $self->compress_series($YAML::CompressSeries)
      if defined $YAML::CompressSeries;
    $self->inline_series($YAML::InlineSeries)
      if defined $YAML::InlineSeries;
    $self->use_aliases($YAML::UseAliases)
      if defined $YAML::UseAliases;
    $self->purity($YAML::Purity)
      if defined $YAML::Purity;
    $self->stringify($YAML::Stringify)
      if defined $YAML::Stringify;
    $self->quote_numeric_strings($YAML::QuoteNumericStrings)
      if defined $YAML::QuoteNumericStrings;
}

sub dump {
    my $self = shift;
    $self->die('dump() not implemented in this class.');
}

sub blessed {
    my $self = shift;
    my ($ref) = @_;
    $ref = \$_[0] unless ref $ref;
    my (undef, undef, $node_id) = YAML::Mo::Object->node_info($ref);
    $self->{blessed_map}->{$node_id};
}

sub bless {
    my $self = shift;
    my ($ref, $blessing) = @_;
    my $ynode;
    $ref = \$_[0] unless ref $ref;
    my (undef, undef, $node_id) = YAML::Mo::Object->node_info($ref);
    if (not defined $blessing) {
        $ynode = YAML::Node->new($ref);
    }
    elsif (ref $blessing) {
        $self->die() unless ynode($blessing);
        $ynode = $blessing;
    }
    else {
        no strict 'refs';
        my $transfer = $blessing . "::yaml_dump";
        $self->die() unless defined &{$transfer};
        $ynode = &{$transfer}($ref);
        $self->die() unless ynode($ynode);
    }
    $self->{blessed_map}->{$node_id} = $ynode;
    my $object = ynode($ynode) or $self->die();
    return $object;
}

1;
