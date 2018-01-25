require '_h2ph_pre.ph';

no warnings qw(redefine misc);

unless(defined(&_ASMARM_SIGCONTEXT_H)) {
    eval 'sub _ASMARM_SIGCONTEXT_H () {1;}' unless defined(&_ASMARM_SIGCONTEXT_H);
}
1;
