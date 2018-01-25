require '_h2ph_pre.ph';

no warnings qw(redefine misc);

unless(defined(&_SYS_UCONTEXT_H)) {
    eval 'sub _SYS_UCONTEXT_H () {1;}' unless defined(&_SYS_UCONTEXT_H);
    require 'features.ph';
    require 'signal.ph';
    require 'bits/sigcontext.ph';
    eval 'sub NGREG () {18;}' unless defined(&NGREG);
    eval("sub REG_R0 () { 0; }") unless defined(&REG_R0);
    eval("sub REG_R1 () { 1; }") unless defined(&REG_R1);
    eval("sub REG_R2 () { 2; }") unless defined(&REG_R2);
    eval("sub REG_R3 () { 3; }") unless defined(&REG_R3);
    eval("sub REG_R4 () { 4; }") unless defined(&REG_R4);
    eval("sub REG_R5 () { 5; }") unless defined(&REG_R5);
    eval("sub REG_R6 () { 6; }") unless defined(&REG_R6);
    eval("sub REG_R7 () { 7; }") unless defined(&REG_R7);
    eval("sub REG_R8 () { 8; }") unless defined(&REG_R8);
    eval("sub REG_R9 () { 9; }") unless defined(&REG_R9);
    eval("sub REG_R10 () { 10; }") unless defined(&REG_R10);
    eval("sub REG_R11 () { 11; }") unless defined(&REG_R11);
    eval("sub REG_R12 () { 12; }") unless defined(&REG_R12);
    eval("sub REG_R13 () { 13; }") unless defined(&REG_R13);
    eval("sub REG_R14 () { 14; }") unless defined(&REG_R14);
    eval("sub REG_R15 () { 15; }") unless defined(&REG_R15);
}
1;
