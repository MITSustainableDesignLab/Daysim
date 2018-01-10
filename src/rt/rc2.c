#ifndef lint
static const char RCSid[] = "$Id: rc2.c,v 2.22 2017/02/07 16:48:14 greg Exp $";
#endif
/*
 * Accumulate ray contributions for a set of materials
 * File i/o and recovery
 */

#include <ctype.h>
#include "platform.h"
#include "rcontrib.h"
#include "resolu.h"

/* Close output stream and free record */
static void
closestream(void *p)
{
	STREAMOUT	*sop = (STREAMOUT *)p;
	
	if (sop->ofp != NULL) {
		int	status = 0;
		if (sop->outpipe)
			status = pclose(sop->ofp);
		else if (sop->ofp != stdout)
			status = fclose(sop->ofp);
		if (status)
			error(SYSTEM, "error closing output stream");
	}
	free(p);
}

LUTAB	ofiletab = LU_SINIT(free,closestream);	/* output file table */

#define OF_MODIFIER	01
#define OF_BIN		02

/************************** STREAM & FILE I/O ***************************/

/* Construct output file name and return flags whether modifier/bin present */
static int
ofname(char *oname, const char *ospec, const char *mname, int bn)
{
	const char	*mnp = NULL;
	const char	*bnp = NULL;
	const char	*cp;
	
	if (ospec == NULL)
		return(-1);
	for (cp = ospec; *cp; cp++)		/* check format position(s) */
		if (*cp == '%') {
			do
				++cp;
			while (isdigit(*cp));
			switch (*cp) {
			case '%':
				break;
			case 's':
				if (mnp != NULL)
					return(-1);
				mnp = cp;
				break;
			case 'd':
			case 'i':
			case 'o':
			case 'x':
			case 'X':
				if (bnp != NULL)
					return(-1);
				bnp = cp;
				break;
			default:
				return(-1);
			}
		}
	if (mnp != NULL) {			/* create file name */
		if (bnp != NULL) {
			if (bnp > mnp)
				sprintf(oname, ospec, mname, bn);
			else
				sprintf(oname, ospec, bn, mname);
			return(OF_MODIFIER|OF_BIN);
		}
		sprintf(oname, ospec, mname);
		return(OF_MODIFIER);
	}
	if (bnp != NULL) {
		sprintf(oname, ospec, bn);
		return(OF_BIN);
	}
	strcpy(oname, ospec);
	return(0);
}


/* Write header to the given output stream */
static void
printheader(FILE *fout, const char *info)
{
	extern char	VersionID[];
						/* copy octree header */
	if (octname[0] == '!') {
		newheader("RADIANCE", fout);
		fputs(octname+1, fout);
		if (octname[strlen(octname)-1] != '\n')
			fputc('\n', fout);
	} else {
		FILE	*fin = fopen(octname, (outfmt=='a') ? "r" : "rb");
		if (fin == NULL)
			quit(1);
		checkheader(fin, OCTFMT, fout);
		fclose(fin);
	}
	printargs(gargc-1, gargv, fout);	/* add our command */
	fprintf(fout, "SOFTWARE= %s\n", VersionID);
	fputnow(fout);
	fputs("NCOMP=3\n", fout);		/* always RGB */
	if (info != NULL)			/* add extra info if given */
		fputs(info, fout);
	fputformat(formstr(outfmt), fout);
	fputc('\n', fout);			/* empty line ends header */
}


/* Write resolution string to given output stream */
static void
printresolu(FILE *fout, int xr, int yr)
{
	if ((xr > 0) & (yr > 0))	/* resolution string */
		fprtresolu(xr, yr, fout);
}


/* Get output stream pointer (open and write header if new and noopen==0) */
STREAMOUT *
getostream(const char *ospec, const char *mname, int bn, int noopen)
{
	static STREAMOUT	stdos;
	char			info[1024];
	int			ofl;
	char			oname[1024];
	LUENT			*lep;
	STREAMOUT		*sop;
	char			*cp;
	
	if (ospec == NULL) {			/* use stdout? */
		if (!noopen & !using_stdout) {
			if (outfmt != 'a')
				SET_FILE_BINARY(stdout);
			if (header) {
				cp = info;
				if (yres > 0) {
					sprintf(cp, "NROWS=%d\n", yres *
							(xres + !xres) );
					while (*cp) ++cp;
				}
				if ((xres <= 0) | (stdos.reclen > 1))
					sprintf(cp, "NCOLS=%d\n", stdos.reclen);
				printheader(stdout, info);
			}
			if (stdos.reclen == 1)
				printresolu(stdout, xres, yres);
			if (waitflush > 0)
				fflush(stdout);
			stdos.xr = xres; stdos.yr = yres;
#ifdef getc_unlocked
			flockfile(stdout);	/* avoid lock/unlock overhead */
#endif
			using_stdout = 1;
		}
		stdos.ofp = stdout;
		stdos.reclen += noopen;
		return(&stdos);
	}
	ofl = ofname(oname, ospec, mname, bn);	/* get output name */
	if (ofl < 0) {
		sprintf(errmsg, "bad output format '%s'", ospec);
		error(USER, errmsg);
	}
	lep = lu_find(&ofiletab, oname);	/* look it up */
	if (lep->key == NULL)			/* new entry */
		lep->key = strcpy((char *)malloc(strlen(oname)+1), oname);
	sop = (STREAMOUT *)lep->data;
	if (sop == NULL) {			/* allocate stream */
		sop = (STREAMOUT *)malloc(sizeof(STREAMOUT));
		if (sop == NULL)
			error(SYSTEM, "out of memory in getostream");
		sop->outpipe = (oname[0] == '!');
		sop->reclen = 0;
		sop->ofp = NULL;		/* open iff noopen==0 */
		sop->xr = xres; sop->yr = yres;
		lep->data = (char *)sop;
		if (!sop->outpipe & !force_open & !recover &&
				access(oname, F_OK) == 0) {
			errno = EEXIST;		/* file exists */
			goto openerr;
		}
	} else if (noopen && outfmt == 'c' &&	/* stream exists to picture? */
			(sop->xr > 0) & (sop->yr > 0)) {
		if (ofl & OF_BIN)
			return(NULL);		/* let caller offset bins */
		sprintf(errmsg, "output '%s' not a valid picture", oname);
		error(WARNING, errmsg);
	}
	if (!noopen && sop->ofp == NULL) {	/* open output stream */
		if (oname[0] == '!')		/* output to command */
			sop->ofp = popen(oname+1, "w");
		else				/* else open file */
			sop->ofp = fopen(oname, "w");
		if (sop->ofp == NULL)
			goto openerr;
		if (outfmt != 'a')
			SET_FILE_BINARY(sop->ofp);
#ifdef getc_unlocked
		flockfile(sop->ofp);		/* avoid lock/unlock overhead */
#endif
		if (accumulate > 0) {		/* global resolution */
			sop->xr = xres; sop->yr = yres;
		}
		if (header) {
			cp = info;
			if (ofl & OF_MODIFIER || sop->reclen == 1) {
				sprintf(cp, "MODIFIER=%s\n", mname);
				while (*cp) ++cp;
			}
			if (ofl & OF_BIN) {
				sprintf(cp, "BIN=%d\n", bn);
				while (*cp) ++cp;
			}
			if (sop->yr > 0) {
				sprintf(cp, "NROWS=%d\n", sop->yr *
						(sop->xr + !sop->xr) );
				while (*cp) ++cp;
			}
			if ((sop->xr <= 0) | (sop->reclen > 1))
				sprintf(cp, "NCOLS=%d\n", sop->reclen);
			printheader(sop->ofp, info);
		}
		if (sop->reclen == 1)
			printresolu(sop->ofp, sop->xr, sop->yr);
		if (waitflush > 0)
			fflush(sop->ofp);
	}
	sop->reclen += noopen;			/* add to length if noopen */
	return(sop);				/* return output stream */
openerr:
	sprintf(errmsg, "cannot open '%s' for writing", oname);
	error(SYSTEM, errmsg);
	return(NULL);	/* pro forma return */
}


/* Get a vector from stdin */
int
getvec(FVECT vec)
{
	float	vf[3];
	double	vd[3];
	char	buf[32];
	int	i;

	switch (inpfmt) {
	case 'a':					/* ascii */
		for (i = 0; i < 3; i++) {
			if (fgetword(buf, sizeof(buf), stdin) == NULL ||
					!isflt(buf))
				return(-1);
			vec[i] = atof(buf);
		}
		break;
	case 'f':					/* binary float */
		if (getbinary((char *)vf, sizeof(float), 3, stdin) != 3)
			return(-1);
		VCOPY(vec, vf);
		break;
	case 'd':					/* binary double */
		if (getbinary((char *)vd, sizeof(double), 3, stdin) != 3)
			return(-1);
		VCOPY(vec, vd);
		break;
	default:
		error(CONSISTENCY, "botched input format");
	}
	return(0);
}


/* Put out ray contribution to file */
static void
put_contrib(const DCOLOR cnt, FILE *fout)
{
	double	sf = 1;
	COLOR	fv;
	COLR	cv;

	if (accumulate > 1)
		sf = 1./(double)accumulate;
	switch (outfmt) {
	case 'a':
		if (accumulate > 1)
			fprintf(fout, "%.6e\t%.6e\t%.6e\t",
					sf*cnt[0], sf*cnt[1], sf*cnt[2]);
		else
			fprintf(fout, "%.6e\t%.6e\t%.6e\t",
					cnt[0], cnt[1], cnt[2]);
		break;
	case 'f':
		if (accumulate > 1) {
			copycolor(fv, cnt);
			scalecolor(fv, sf);
		} else
			copycolor(fv, cnt);
		putbinary(fv, sizeof(float), 3, fout);
		break;
	case 'd':
		if (accumulate > 1) {
			DCOLOR	dv;
			copycolor(dv, cnt);
			scalecolor(dv, sf);
			putbinary(dv, sizeof(double), 3, fout);
		} else
			putbinary(cnt, sizeof(double), 3, fout);
		break;
	case 'c':
		if (accumulate > 1)
			setcolr(cv, sf*cnt[0], sf*cnt[1], sf*cnt[2]);
		else
			setcolr(cv, cnt[0], cnt[1], cnt[2]);
		putbinary(cv, sizeof(cv), 1, fout);
		break;
	default:
		error(INTERNAL, "botched output format");
	}
}


/* Output modifier values to appropriate stream(s) */
void
mod_output(MODCONT *mp)
{
	STREAMOUT	*sop = getostream(mp->outspec, mp->modname, mp->bin0, 0);
	int		j;

	put_contrib(mp->cbin[0], sop->ofp);
	if (mp->nbins > 3 &&	/* minor optimization */
			sop == getostream(mp->outspec, mp->modname, mp->bin0+1, 0)) {
		for (j = 1; j < mp->nbins; j++)
			put_contrib(mp->cbin[j], sop->ofp);
	} else {
		for (j = 1; j < mp->nbins; j++) {
			sop = getostream(mp->outspec, mp->modname, mp->bin0+j, 0);
			put_contrib(mp->cbin[j], sop->ofp);
		}
	}
}


/* callback to output newline to ASCII file and/or flush as requested */
static int
puteol(const LUENT *e, void *p)
{
	STREAMOUT	*sop = (STREAMOUT *)e->data;

	if (outfmt == 'a')
		putc('\n', sop->ofp);
	if (!waitflush)
		fflush(sop->ofp);
	if (ferror(sop->ofp)) {
		sprintf(errmsg, "write error on file '%s'", e->key);
		error(SYSTEM, errmsg);
	}
	return(0);
}


/* Terminate record output and flush if time */
void
end_record()
{
	--waitflush;
	lu_doall(&ofiletab, &puteol, NULL);
	if (using_stdout & (outfmt == 'a'))
		putc('\n', stdout);
	if (!waitflush) {
		waitflush = (yres > 0) & (xres > 1) ? 0 : xres;
		if (using_stdout)
			fflush(stdout);
	}
}

/************************** PARTIAL RESULTS RECOVERY ***********************/

/* Get ray contribution from previous file */
static int
get_contrib(DCOLOR cnt, FILE *finp)
{
	COLOR	fv;
	COLR	cv;

	switch (outfmt) {
	case 'a':
		return(fscanf(finp,"%lf %lf %lf",&cnt[0],&cnt[1],&cnt[2]) == 3);
	case 'f':
		if (getbinary(fv, sizeof(fv[0]), 3, finp) != 3)
			return(0);
		copycolor(cnt, fv);
		return(1);
	case 'd':
		return(getbinary(cnt, sizeof(cnt[0]), 3, finp) == 3);
	case 'c':
		if (getbinary(cv, sizeof(cv), 1, finp) != 1)
			return(0);
		colr_color(fv, cv);
		copycolor(cnt, fv);
		return(1);
	default:
		error(INTERNAL, "botched output format");
	}
	return(0);	/* pro forma return */
}


/* Close output file opened for input */
static int
myclose(const LUENT *e, void *p)
{
	STREAMOUT	*sop = (STREAMOUT *)e->data;
	
	if (sop->ofp == NULL)
		return(0);
	fclose(sop->ofp);
	sop->ofp = NULL;
	return(0);
}


/* Load previously accumulated values */
void
reload_output()
{
	int		i, j;
	MODCONT		*mp;
	int		ofl;
	char		oname[1024];
	char		*fmode = "rb";
	char		*outvfmt;
	LUENT		*oent;
	int		xr, yr;
	STREAMOUT	*sop;
	DCOLOR		rgbv;

	if (outfmt == 'a')
		fmode = "r";
	outvfmt = formstr(outfmt);
						/* reload modifier values */
	for (i = 0; i < nmods; i++) {
		mp = (MODCONT *)lu_find(&modconttab,modname[i])->data;
		if (mp->outspec == NULL)
			error(USER, "cannot reload from stdout");
		if (mp->outspec[0] == '!')
			error(USER, "cannot reload from command");
		for (j = 0; j < mp->nbins; j++) { /* load each modifier bin */
			ofl = ofname(oname, mp->outspec, mp->modname, mp->bin0+j);
			if (ofl < 0)
				error(USER, "bad output file specification");
			oent = lu_find(&ofiletab, oname);
			if (oent->data == NULL)
				error(INTERNAL, "unallocated stream in reload_output()");
			sop = (STREAMOUT *)oent->data;
			if (sop->ofp == NULL) {	/* open output as input */
				sop->ofp = fopen(oname, fmode);
				if (sop->ofp == NULL) {
					sprintf(errmsg, "missing reload file '%s'",
							oname);
					error(WARNING, errmsg);
					break;
				}
#ifdef getc_unlocked
				flockfile(sop->ofp);
#endif
				if (header && checkheader(sop->ofp, outvfmt, NULL) != 1) {
					sprintf(errmsg, "format mismatch for '%s'",
							oname);
					error(USER, errmsg);
				}
				if ((sop->reclen == 1) & (sop->xr > 0) & (sop->yr > 0) &&
						(!fscnresolu(&xr, &yr, sop->ofp) ||
							(xr != sop->xr) |
							(yr != sop->yr))) {
					sprintf(errmsg, "resolution mismatch for '%s'",
							oname);
					error(USER, errmsg);
				}
			}
							/* read in RGB value */
			if (!get_contrib(rgbv, sop->ofp)) {
				if (!j) {
					fclose(sop->ofp);
					break;		/* ignore empty file */
				}
				if (j < mp->nbins) {
					sprintf(errmsg, "missing data in '%s'",
							oname);
					error(USER, errmsg);
				}
				break;
			}				
			copycolor(mp->cbin[j], rgbv);
		}
	}
	lu_doall(&ofiletab, &myclose, NULL);	/* close all files */
}


/* Seek on the given output file */
static int
myseeko(const LUENT *e, void *p)
{
	STREAMOUT	*sop = (STREAMOUT *)e->data;
	off_t		nbytes = *(off_t *)p;
	
	if (sop->reclen > 1)
		nbytes *= (off_t)sop->reclen;
	if (fseeko(sop->ofp, nbytes, SEEK_CUR) < 0) {
		sprintf(errmsg, "seek error on file '%s'", e->key);
		error(SYSTEM, errmsg);
	}
	return(0);
}


/* Recover output if possible */
void
recover_output()
{
	off_t		lastout = -1;
	int		outvsiz, recsiz;
	char		*outvfmt;
	int		i, j;
	MODCONT		*mp;
	int		ofl;
	char		oname[1024];
	LUENT		*oent;
	STREAMOUT	*sop;
	off_t		nvals;
	int		xr, yr;

	switch (outfmt) {
	case 'a':
		error(USER, "cannot recover ASCII output");
		return;
	case 'f':
		outvsiz = sizeof(float)*3;
		break;
	case 'd':
		outvsiz = sizeof(double)*3;
		break;
	case 'c':
		outvsiz = sizeof(COLR);
		break;
	default:
		error(INTERNAL, "botched output format");
		return;
	}
	outvfmt = formstr(outfmt);
						/* check modifier outputs */
	for (i = 0; i < nmods; i++) {
		mp = (MODCONT *)lu_find(&modconttab,modname[i])->data;
		if (mp->outspec == NULL)
			error(USER, "cannot recover from stdout");
		if (mp->outspec[0] == '!')
			error(USER, "cannot recover from command");
		for (j = 0; j < mp->nbins; j++) { /* check each bin's file */
			ofl = ofname(oname, mp->outspec, mp->modname, mp->bin0+j);
			if (ofl < 0)
				error(USER, "bad output file specification");
			oent = lu_find(&ofiletab, oname);
			if (oent->data == NULL)
				error(INTERNAL, "unallocated stream in recover_output()");
			sop = (STREAMOUT *)oent->data;
			if (sop->ofp != NULL) {	/* already open? */
				if (ofl & OF_BIN)
					continue;
				break;
			}
						/* open output */
			sop->ofp = fopen(oname, "rb+");
			if (sop->ofp == NULL) {
				sprintf(errmsg, "missing recover file '%s'",
						oname);
				error(WARNING, errmsg);
				lastout = 0;
				break;
			}
			nvals = lseek(fileno(sop->ofp), 0, SEEK_END);
			if (nvals <= 0) {
				lastout = 0;	/* empty output, quit here */
				fclose(sop->ofp);
				break;
			}
			recsiz = outvsiz * sop->reclen;

			lseek(fileno(sop->ofp), 0, SEEK_SET);
			if (header && checkheader(sop->ofp, outvfmt, NULL) != 1) {
				sprintf(errmsg, "format mismatch for '%s'",
						oname);
				error(USER, errmsg);
			}
			if ((sop->reclen == 1) & (sop->xr > 0) & (sop->yr > 0) &&
					(!fscnresolu(&xr, &yr, sop->ofp) ||
						(xr != sop->xr) |
						(yr != sop->yr))) {
				sprintf(errmsg, "resolution mismatch for '%s'",
						oname);
				error(USER, errmsg);
			}
			nvals = (nvals - (off_t)ftell(sop->ofp)) / recsiz;
			if ((lastout < 0) | (nvals < lastout))
				lastout = nvals;
			if (!(ofl & OF_BIN))
				break;		/* no bin separation */
		}
		if (!lastout) {			/* empty output */
			error(WARNING, "no previous data to recover");
						/* reclose all outputs */
			lu_doall(&ofiletab, &myclose, NULL);
			return;
		}
	}
	if (lastout < 0) {
		error(WARNING, "no output files to recover");
		return;
	}
	if (raysleft && lastout >= raysleft/accumulate) {
		error(WARNING, "output appears to be complete");
		/* XXX should read & discard input? */
		quit(0);
	}
						/* seek on all files */
	nvals = lastout * outvsiz;
	lu_doall(&ofiletab, &myseeko, &nvals);
						/* skip repeated input */
	lastout *= accumulate;
	for (nvals = 0; nvals < lastout; nvals++) {
		FVECT	vdummy;
		if (getvec(vdummy) < 0 || getvec(vdummy) < 0)
			error(USER, "unexpected EOF on input");
	}
	lastray = lastdone = (RNUMBER)lastout;
	if (raysleft)
		raysleft -= lastray;
}
