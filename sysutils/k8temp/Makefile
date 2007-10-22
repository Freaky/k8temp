# New ports collection makefile for:    k8temp
# Date created:        21 October 2007
# Whom:                Thomas Hurst
#
# $FreeBSD$
#

PORTNAME=	k8temp
PORTVERSION=	0.1.1
CATEGORIES=	sysutils
MASTER_SITES=	http://hur.st/k8temp/

MAINTAINER=	tom@hur.st
COMMENT=	Athlon 64 and Opteron on-die temperature reader

MAN8=	k8temp.8
MANCOMPRESSED=	yes

PLIST_FILES=	sbin/k8temp

do-install:
	${INSTALL_PROGRAM} ${WRKSRC}/k8temp ${PREFIX}/sbin

.include <bsd.port.mk>
