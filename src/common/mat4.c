/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  mat4.c - routines dealing with 4 X 4 homogeneous transformation matrices.
 *
 *     10/19/85
 */


static double  m4tmp[4][4];		/* for efficiency */

#define  copymat4(m4a,m4b)	bcopy((char *)m4b,(char *)m4a,sizeof(m4tmp))


setident4(m4)
double  m4[4][4];
{
	static double  ident[4][4] = {
		1.,0.,0.,0.,
		0.,1.,0.,0.,
		0.,0.,1.,0.,
		0.,0.,0.,1.,
	};
	copymat4(m4, ident);
}


multmat4(m4a, m4b, m4c)		/* multiply m4b X m4c and put into m4a */
double  m4a[4][4];
register double  m4b[4][4], m4c[4][4];
{
	register int  i, j;
	
	for (i = 4; i--; )
		for (j = 4; j--; )
			m4tmp[i][j] = m4b[i][0]*m4c[0][j] +
				      m4b[i][1]*m4c[1][j] +
				      m4b[i][2]*m4c[2][j] +
				      m4b[i][3]*m4c[3][j];
	
	copymat4(m4a, m4tmp);
}


multv3(v3a, v3b, m4)	/* transform vector v3b by m4 and put into v3a */
double  v3a[3];
register double  v3b[3];
register double  m4[4][4];
{
	m4tmp[0][0] = v3b[0]*m4[0][0] + v3b[1]*m4[1][0] + v3b[2]*m4[2][0];
	m4tmp[0][1] = v3b[0]*m4[0][1] + v3b[1]*m4[1][1] + v3b[2]*m4[2][1];
	m4tmp[0][2] = v3b[0]*m4[0][2] + v3b[1]*m4[1][2] + v3b[2]*m4[2][2];
	
	v3a[0] = m4tmp[0][0];
	v3a[1] = m4tmp[0][1];
	v3a[2] = m4tmp[0][2];
}


multp3(p3a, p3b, m4)		/* transform p3b by m4 and put into p3a */
register double  p3a[3];
double  p3b[3];
register double  m4[4][4];
{
	multv3(p3a, p3b, m4);	/* transform as vector */
	p3a[0] += m4[3][0];	/* translate */
	p3a[1] += m4[3][1];
	p3a[2] += m4[3][2];
}


#ifdef  INVMAT
/*
 * invmat - computes the inverse of mat into inverse.  Returns 1
 * if there exists an inverse, 0 otherwise.  It uses Gaussian Elimination
 * method with partial pivoting.
 */

invmat(inverse,mat)
double mat[4][4],inverse[4][4];
{
#define SWAP(a,b,t) (t=a,a=b,b=t)
#define ABS(x) (x>=0?x:-(x))

	register int i,j,k;
	register double temp;

	copymat4(m4tmp, mat);
	setident(inverse);

	for(i = 0; i < 4; i++) {
		/* Look for row with largest pivot and swap rows */
		temp = 0; j = -1;
		for(k = i; k < 4; k++)
			if(ABS(m4tmp[k][i]) > temp) {
				temp = ABS(m4tmp[k][i]);
				j = k;
				}
		if(j == -1)	/* No replacing row -> no inverse */
			return(0);
		if (j != i)
			for(k = 0; k < 4; k++) {
				SWAP(m4tmp[i][k],m4tmp[j][k],temp);
				SWAP(inverse[i][k],inverse[j][k],temp);
				}

		temp = m4tmp[i][i];
		for(k = 0; k < 4; k++) {
			m4tmp[i][k] /= temp;
			inverse[i][k] /= temp;
			}
		for(j = 0; j < 4; j++) {
			if(j != i) {
				temp = m4tmp[j][i];
				for(k = 0; k < 4; k++) {
					m4tmp[j][k] -= m4tmp[i][k]*temp;
					inverse[j][k] -= inverse[i][k]*temp;
					}
				}
			}
		}
	return(1);

#undef ABS
#undef SWAP
}
#endif
