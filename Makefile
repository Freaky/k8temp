
PREFIX?=/usr/local

WARNS=6
CSTD=gnu99

BINDIR=${PREFIX}/sbin
MAN=  k8temp.8
PROG= k8temp

SRCS= k8temp.c


.if defined(WITHOUT_PCIOCGETCONF) || defined(OPENBSD)
CFLAGS+= -DWITHOUT_PCIOCGETCONF
.endif

.if !defined(WITHOUT_DEVPCI)
SRCS+= k8temp_devpci.c
.else
CFLAGS+= -DWITHOUT_DEVPCI
.endif


.include "bsd.prog.mk"
