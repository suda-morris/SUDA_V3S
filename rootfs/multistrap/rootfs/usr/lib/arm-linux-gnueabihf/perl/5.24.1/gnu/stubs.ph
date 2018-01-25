require '_h2ph_pre.ph';

no warnings qw(redefine misc);

if(!defined (&__ARM_PCS_VFP)) {
    require 'gnu/stubs-soft.ph';
}
if(defined (&__ARM_PCS_VFP)) {
    require 'gnu/stubs-hard.ph';
}
1;
