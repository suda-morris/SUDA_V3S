require '_h2ph_pre.ph';

no warnings qw(redefine misc);

unless(defined(&_BITS_PTHREADTYPES_H)) {
    eval 'sub _BITS_PTHREADTYPES_H () {1;}' unless defined(&_BITS_PTHREADTYPES_H);
    require 'endian.ph';
    eval 'sub __SIZEOF_PTHREAD_ATTR_T () {36;}' unless defined(&__SIZEOF_PTHREAD_ATTR_T);
    eval 'sub __SIZEOF_PTHREAD_MUTEX_T () {24;}' unless defined(&__SIZEOF_PTHREAD_MUTEX_T);
    eval 'sub __SIZEOF_PTHREAD_MUTEXATTR_T () {4;}' unless defined(&__SIZEOF_PTHREAD_MUTEXATTR_T);
    eval 'sub __SIZEOF_PTHREAD_COND_T () {48;}' unless defined(&__SIZEOF_PTHREAD_COND_T);
    eval 'sub __SIZEOF_PTHREAD_COND_COMPAT_T () {12;}' unless defined(&__SIZEOF_PTHREAD_COND_COMPAT_T);
    eval 'sub __SIZEOF_PTHREAD_CONDATTR_T () {4;}' unless defined(&__SIZEOF_PTHREAD_CONDATTR_T);
    eval 'sub __SIZEOF_PTHREAD_RWLOCK_T () {32;}' unless defined(&__SIZEOF_PTHREAD_RWLOCK_T);
    eval 'sub __SIZEOF_PTHREAD_RWLOCKATTR_T () {8;}' unless defined(&__SIZEOF_PTHREAD_RWLOCKATTR_T);
    eval 'sub __SIZEOF_PTHREAD_BARRIER_T () {20;}' unless defined(&__SIZEOF_PTHREAD_BARRIER_T);
    eval 'sub __SIZEOF_PTHREAD_BARRIERATTR_T () {4;}' unless defined(&__SIZEOF_PTHREAD_BARRIERATTR_T);
    unless(defined(&__have_pthread_attr_t)) {
	eval 'sub __have_pthread_attr_t () {1;}' unless defined(&__have_pthread_attr_t);
    }
    eval 'sub __PTHREAD_SPINS () {0;}' unless defined(&__PTHREAD_SPINS);
    if(defined (&__USE_UNIX98) || defined (&__USE_XOPEN2K)) {
	if((defined(&__BYTE_ORDER) ? &__BYTE_ORDER : undef) == (defined(&__BIG_ENDIAN) ? &__BIG_ENDIAN : undef)) {
	} else {
	}
	eval 'sub __PTHREAD_RWLOCK_ELISION_EXTRA () {0;}' unless defined(&__PTHREAD_RWLOCK_ELISION_EXTRA);
    }
    if(defined(&__USE_XOPEN2K)) {
    }
}
1;
