package YAML::Loader::Base;

use YAML::Mo;

has load_code     => default => sub {0};
has preserve      => default => sub {0};
has stream        => default => sub {''};
has document      => default => sub {0};
has line          => default => sub {0};
has documents     => default => sub {[]};
has lines         => default => sub {[]};
has eos           => default => sub {0};
has done          => default => sub {0};
has anchor2node   => default => sub {{}};
has level         => default => sub {0};
has offset        => default => sub {[]};
has preface       => default => sub {''};
has content       => default => sub {''};
has indent        => default => sub {0};
has major_version => default => sub {0};
has minor_version => default => sub {0};
has inline        => default => sub {''};
has numify        => default => sub {0};

sub set_global_options {
    my $self = shift;
    $self->load_code($YAML::LoadCode || $YAML::UseCode)
      if defined $YAML::LoadCode or defined $YAML::UseCode;
    $self->preserve($YAML::Preserve) if defined $YAML::Preserve;
    $self->numify($YAML::Numify) if defined $YAML::Numify;
}

sub load {
    die 'load() not implemented in this class.';
}

1;
