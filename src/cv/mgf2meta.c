/* Copyright (c) 1995 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Convert MGF (Materials and Geometry Format) to Metafile 2-d graphics
 */

#include <stdio.h>
#include <math.h>
#include "random.h"
#include "mgflib/parser.h"

#define	MX(v)	(int)(((1<<14)-1)*(v)[(proj_axis+1)%3])
#define	MY(v)	(int)(((1<<14)-1)*(v)[(proj_axis+2)%3])

int	r_face();
int	proj_axis;
double	limit[3][2];
int	layer;

extern int	mg_nqcdivs;


main(argc, argv)		/* convert files to stdout */
int	argc;
char	*argv[];
{
	int	i;
				/* initialize dispatch table */
	mg_ehand[MG_E_FACE] = r_face;
	mg_ehand[MG_E_POINT] = c_hvertex;
	mg_ehand[MG_E_VERTEX] = c_hvertex;
	mg_ehand[MG_E_XF] = xf_handler;
	mg_nqcdivs = 3;		/* reduce object subdivision */
	mg_init();		/* initialize the parser */
					/* get arguments */
	if (argc < 8 || (proj_axis = argv[1][0]-'x') < 0 || proj_axis > 2)
		goto userr;
	limit[0][0] = atof(argv[2]); limit[0][1] = atof(argv[3]);
	limit[1][0] = atof(argv[4]); limit[1][1] = atof(argv[5]);
	limit[2][0] = atof(argv[6]); limit[2][1] = atof(argv[7]);
	
	if (argc == 8) {		/* convert stdin */
		if (mg_load(NULL) != MG_OK)
			exit(1);
	} else				/* convert each file */
		for (i = 8; i < argc; i++) {
			if (mg_load(argv[i]) != MG_OK)
				exit(1);
			newlayer();
		}
	mendpage();			/* print page */
	mdone();			/* close output */
	exit(0);
userr:
	fprintf(stderr, "Usage: %s {x|y|z} xmin xmax ymin ymax zmin zmax [file.mgf] ..\n",
			argv[0]);
	exit(1);
}


int
r_face(ac, av)			/* convert a face */
int	ac;
char	**av;
{
	static FVECT	bbmin = {0,0,0}, bbmax = {1,1,1};
	register int	i, j;
	register C_VERTEX	*cv;
	FVECT	v1, v2, vo;

	if (ac < 4)
		return(MG_EARGC);
						/* connect to last point */
	if ((cv = c_getvert(av[ac-1])) == NULL)
		return(MG_EUNDEF);
	xf_xfmpoint(vo, cv->p);
	for (j = 0; j < 3; j++)
		vo[j] = (vo[j] - limit[j][0])/(limit[j][1]-limit[j][0]);
	for (i = 1; i < ac; i++) {		/* go around face */
		if ((cv = c_getvert(av[i])) == NULL)
			return(MG_EUNDEF);
		xf_xfmpoint(v2, cv->p);
		for (j = 0; j < 3; j++)
			v2[j] = (v2[j] - limit[j][0])/(limit[j][1]-limit[j][0]);
		VCOPY(v1, vo);
		VCOPY(vo, v2);
		if (clip(v1, v2, bbmin, bbmax))
			doline(MX(v1), MY(v1), MX(v2), MY(v2));
	}
	return(MG_OK);
}


#define  HTBLSIZ	16381		/* prime hash table size */

short	hshtab[HTBLSIZ][4];		/* done line segments */

#define  hash(mx1,my1,mx2,my2)	((long)(mx1)<<15 ^ (long)(my1)<<10 ^ \
					(long)(mx2)<<5 ^ (long)(my2))

#define  RANDMASK	((1L<<14)-1)


newlayer()				/* start a new layer */
{
#ifdef BSD
	bzero((char *)hshtab, sizeof(hshtab));
#else
	(void)memset((char *)hshtab, 0, sizeof(hshtab));
#endif
	if (++layer >= 16) {
		mendpage();
		layer = 0;
	}
}


int
doline(v1x, v1y, v2x, v2y)		/* draw line conditionally */
int	v1x, v1y, v2x, v2y;
{
	register int	h;

	if (v1x > v2x || (v1x == v2x && v1y > v2y)) {	/* sort endpoints */
		h=v1x; v1x=v2x; v2x=h;
		h=v1y; v1y=v2y; v2y=h;
	}
	h = hash(v1x, v1y, v2x, v2y) % HTBLSIZ;
	if (hshtab[h][0] == v1x && hshtab[h][1] == v1y &&
			hshtab[h][2] == v2x && hshtab[h][3] == v2y)
		return(0);
	hshtab[h][0] = v1x; hshtab[h][1] = v1y;
	hshtab[h][2] = v2x; hshtab[h][3] = v2y;
	if ((long)(v2x-v1x)*(v2x-v1x) + (long)(v2y-v1y)*(v2y-v1y)
			<= (random()&RANDMASK))
		return(0);
	mline(v1x, v1y, layer/4, 0, layer%4);
	mdraw(v2x, v2y);
	return(1);
}
