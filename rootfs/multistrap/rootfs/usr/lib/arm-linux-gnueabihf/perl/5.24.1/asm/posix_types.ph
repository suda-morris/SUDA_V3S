require '_h2ph_pre.ph';

no warnings qw(redefine misc);

unless(defined(&__ARCH_ARM_POSIX_TYPES_H)) {
    eval 'sub __ARCH_ARM_POSIX_TYPES_H () {1;}' unless defined(&__ARCH_ARM_POSIX_TYPES_H);
    require 'asm-generic/posix_types.ph';
}
1;
