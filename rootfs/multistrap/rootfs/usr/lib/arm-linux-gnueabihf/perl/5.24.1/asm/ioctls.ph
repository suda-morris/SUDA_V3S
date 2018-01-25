require '_h2ph_pre.ph';

no warnings qw(redefine misc);

unless(defined(&__ASM_ARM_IOCTLS_H)) {
    eval 'sub __ASM_ARM_IOCTLS_H () {1;}' unless defined(&__ASM_ARM_IOCTLS_H);
    eval 'sub FIOQSIZE () {0x545e;}' unless defined(&FIOQSIZE);
    require 'asm-generic/ioctls.ph';
}
1;
