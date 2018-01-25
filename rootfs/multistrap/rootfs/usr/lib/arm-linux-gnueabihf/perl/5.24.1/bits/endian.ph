require '_h2ph_pre.ph';

no warnings qw(redefine misc);

unless(defined(&_ENDIAN_H)) {
    die("Never use <bits/endian.h> directly; include <endian.h> instead.");
}
if(defined(&__ARMEB__)) {
    eval 'sub __BYTE_ORDER () { &__BIG_ENDIAN;}' unless defined(&__BYTE_ORDER);
} else {
    eval 'sub __BYTE_ORDER () { &__LITTLE_ENDIAN;}' unless defined(&__BYTE_ORDER);
}
1;
