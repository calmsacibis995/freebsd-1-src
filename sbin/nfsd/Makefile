#	@(#)Makefile	5.3 (Berkeley) 6/29/90

PROG=	nfsd
CFLAGS+=-DNFS
MAN8=	nfsd.8
DPADD=	${LIBRPC}
LDADD=	-lrpc -lutil

.include <bsd.prog.mk>
