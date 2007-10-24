
PREFIX?=/usr/local

WARNS=6
CSTD=gnu99

BINDIR=${PREFIX}/sbin
MAN=  k8temp.8
PROG= k8temp

.include "bsd.prog.mk"
