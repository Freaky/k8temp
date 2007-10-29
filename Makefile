
PREFIX?= /usr/local
UNAME?=  /usr/bin/uname

.if !defined(OPSYS)
OPSYS!= ${UNAME} -s
.endif

WARNS= 6
CSTD=  gnu99

BINDIR=${PREFIX}/sbin
MAN=  k8temp.8
PROG= k8temp

SRCS= k8temp.c

.if ${OPSYS} == "NetBSD" ||  defined(WITH_LIBPCI)
SRCS+= k8temp_libpci.c
CFLAGS+= -DWITH_LIBPCI
.else
SRCS+= k8temp_devpci.c
. if ${OPSYS} == "OpenBSD" || defined(WITHOUT_PCIOCGETCONF)
CFLAGS+= -DWITHOUT_PCIOCGETCONF
. endif
.endif

.include "bsd.prog.mk"
