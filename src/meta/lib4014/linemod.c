#ifndef lint
static const char	RCSid[] = "$Id$";
#endif

#include "local4014.h"
#include "lib4014.h"

extern void
linemod(
	char *s
)
{
	char c;
	putch(033);
	switch(s[0]){
	case 'l':	
		c = 'd';
		break;
	case 'd':	
		if(s[3] != 'd')c='a';
		else c='b';
		break;
	case 's':
		if(s[5] != '\0')c='c';
		else c='`';
	}
	putch(c);
}
