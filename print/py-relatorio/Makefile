# Created by: Wen Heping <wenheping@gmail.com>
# $FreeBSD$

PORTNAME=	relatorio
PORTVERSION=	0.6.2
CATEGORIES=	print python
MASTER_SITES=	CHEESESHOP
PKGNAMEPREFIX=	${PYTHON_PKGNAMEPREFIX}

MAINTAINER=	ports@FreeBSD.org
COMMENT=	Templating library able to output odt and pdf files

BUILD_DEPENDS=	${PYTHON_PKGNAMEPREFIX}Genshi>=0.5:textproc/py-genshi \
		${PYTHON_PKGNAMEPREFIX}lxml>=2.2:devel/py-lxml
RUN_DEPENDS=	${PYTHON_PKGNAMEPREFIX}pycha>=0.5.0:graphics/py-pycha \
		${PYTHON_PKGNAMEPREFIX}yaml>=3.07:devel/py-yaml

USES=		python
USE_PYTHON=	distutils autoplist

.include <bsd.port.mk>
