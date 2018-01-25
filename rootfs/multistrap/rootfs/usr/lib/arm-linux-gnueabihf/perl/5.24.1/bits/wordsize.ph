require '_h2ph_pre.ph';

no warnings qw(redefine misc);

eval 'sub __WORDSIZE () {32;}' unless defined(&__WORDSIZE);
1;
