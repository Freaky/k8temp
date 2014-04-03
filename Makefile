
PREFIX?= /usr/local
UNAME?=  /usr/bin/uname

.if !defined(OPSYS)
OPSYS!= ${UNAME} -s
.endif

WARNS?= 6
CSTD=  gnu99

BINDIR=${PREFIX}/sbin
MAN=  k8temp.8
PROG= k8temp

SRCS= k8temp.c k8temp.h k8temp_pci.h

.if ${OPSYS} == "NetBSD" || defined(WITH_LIBPCI)
SRCS+= k8temp_libpci.c k8temp_libpci.h
CFLAGS+= -DWITH_LIBPCI
.else
SRCS+= k8temp_devpci.c k8temp_devpci.h
. if ${OPSYS} == "OpenBSD" || defined(WITHOUT_PCIOCGETCONF)
CFLAGS+= -DWITHOUT_PCIOCGETCONF
. endif
.endif

.include "bsd.prog.mk"
