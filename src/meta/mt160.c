#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 *  Program to print meta-files on a dot-matrix printer
 *
 *  cc -o mt160 mt160.c mplot.o plot.o mfio.o syscalls.o palloc.o misc.o
 *
 *  Mannesman Tally MT160 high-speed
 */


#define  MAXALLOC  5000

#define  DXSIZE  532		/* x resolution */

#define  DYSIZE  512		/* y resolution */

#define  LINWIDT  DXSIZE	/* line width */

#define  LINHITE  8		/* line height */

#define  NLINES  (DYSIZE/LINHITE)	/* number of lines to plot */

#define  CHARWIDTH  4

#define  PNORM  "\033[0y\033[6w"

#define  PINIT  "\033[6~\033[7z\033[0y\033[6w"

#define  PUNINIT  "\033[6~"

#define  DBLON  "\033[=z"

#define  DBLOFF  "\033[>z"

#define  OUTPUT  "\033%5"

#define  XCOM  "pexpand +vOCIsp %s | psort -Y +x"


#include  "rtprocess.h"
#include  "meta.h"
#include  "plot.h"
#include  "span.h"


char  *progname;

struct span  outspan;

int  dxsize = DXSIZE, dysize = DYSIZE,
     linwidt = LINWIDT, linhite = LINHITE,
     nrows = (LINHITE-1)/8+1;

int  maxalloc = MAXALLOC;

int  spanmin = 0, spanmax = LINWIDT-1;

int  charwidth = CHARWIDTH;

static char  chrtype[16][5] = {
				"\033[4w",
				"\033[0w",
				"\033[4w",
				"\033[0w",
				"\033[5w",
				"\033[1w",
				"\033[5w",
				"\033[1w",
				"\033[6w",
				"\033[2w",
				"\033[6w",
				"\033[2w",
				"\033[7w",
				"\033[3w",
				"\033[7w",
				"\033[3w"
				};

static int  lineno = 0; 

static short  condonly = FALSE,
	      conditioned = FALSE;


int
main(
	int  argc,
	char  **argv
)
{
 FILE  *fp;
 char  comargs[200], command[300];

 progname = *argv++;
 argc--;

 condonly = FALSE;
 conditioned = FALSE;
 
 while (argc && **argv == '-')  {
    switch (*(*argv+1))  {
       case 'c':
	  condonly = TRUE;
	  break;
       case 'r':
	  conditioned = TRUE;
	  break;
       default:
	  error(WARNING, "unknown option");
	  break;
       }
    argv++;
    argc--;
    }

 if (conditioned) {
    fputs(PINIT, stdout);
    if (argc)
       while (argc)  {
	  fp = efopen(*argv, "r");
	  plot(fp);
	  fclose(fp);
	  argv++;
	  argc--;
	  }
    else
       plot(stdin);
    if (lineno)
	nextpage();
    fputs(PUNINIT, stdout);
 } else  {
    comargs[0] = '\0';
    while (argc)  {
       strcat(comargs, " ");
       strcat(comargs, *argv);
       argv++;
       argc--;
       }
    sprintf(command, XCOM, comargs);
    if (condonly)
       return(system(command));
    else  {
       fputs(PINIT, stdout);
       if ((fp = popen(command, "r")) == NULL)
          error(SYSTEM, "cannot execute input filter");
       plot(fp);
       pclose(fp);
       if (lineno)
	  nextpage();
       fputs(PUNINIT, stdout);
       }
    }

 return(0);
 }


void
thispage(void)		/* rewind and initialize current page */
{
    if (lineno)
	error(USER, "cannot restart page in thispage");
}


void
nextpage(void)		/* advance to next page */
{
    fputs("\f\r", stdout);

    lineno = 0;
}


void
contpage(void)		/* continue new plot on current page */
{
    while (lineno++ < NLINES)
        putc('\n', stdout);
    
    lineno = 0;
}


void
printspan(void)		/* output span to printer */
{
    register int i;

    if (spanmin <= spanmax) {

	i = spanmin/charwidth;
	while (i--)
	    putc(' ', stdout);

	i = spanmin%charwidth;
	fputs(OUTPUT, stdout);
	putc((spanmax-spanmin+i+1)%256, stdout);
	putc((spanmax-spanmin+i+1)/256, stdout);
	while (i--)
	    putc('\0', stdout);

	for (i = spanmin; i <= spanmax; i++)
	    putc(outspan.cols[i], stdout);

	putc('\r', stdout);
    }

    putc('\n', stdout);
    lineno++;
}


void
printstr(		/* output a string to the printer */
PRIMITIVE  *p
)
{
    int  i;

    i = CONV(p->xy[XMN], dxsize)/charwidth;
    while (i--)
	putc(' ', stdout);

    if (p->arg0 & 0100)		/* double strike */
    	fputs(DBLON, stdout);
    else
    	fputs(DBLOFF, stdout);
    	
    fputs(chrtype[(p->arg0 >> 2) & 017], stdout);
    fputs(p->args, stdout);
    fputs(PNORM, stdout);
    putc('\r', stdout);

}
