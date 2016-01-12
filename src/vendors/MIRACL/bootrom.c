/*************************************************************************
                                                                         *
Copyright (c) 2015>, MIRACL Ltd                                *
All rights reserved.                                                     *
                                                                         *
This file is derived from the MIRACL for Ara SDK.                *
                                                                         *
The MIRACL for Ara SDK provides developers with an               *
extensive and efficient set of cryptographic functions.                  *
For further information about its features and functionalities           *
please refer to https://www.miracl.com                                  *
                                                                         *
Redistribution and use in source and binary forms, with or without       *
modification, are permitted provided that the following conditions are   *
met:                                                                     *
                                                                         *
 1. Redistributions of source code must retain the above copyright       *
    notice, this list of conditions and the following disclaimer.        *
                                                                         *
 2. Redistributions in binary form must reproduce the above copyright    *
    notice, this list of conditions and the following disclaimer in the  *
    documentation and/or other materials provided with the distribution. *
                                                                         *
 3. Neither the name of the copyright holder nor the names of its        *
    contributors may be used to endorse or promote products derived      *
    from this software without specific prior written permission.        *
                                                                         *
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS  *
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED    *
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A          *
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT       *
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,   *
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED *
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR   *
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF   *
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING     *
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS       *
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.             *
                                                                         *
**************************************************************************/


/* tiny RSA signature verification program

	Consider a boot rom that checks the signature on an app before it loads it.
	Assume that an RSA digital signature is applied to a hash of the code.

	This code:
	Uses exponent of 3 or 65537
	Uses SHA256 hashing and RSA2048
	Uses PKCS1 v1.5 padding
	Suitable even for small 8-bit processors

	Note that RSA Signature verification (or encryption) is often realistically the only Public Key functionality that can be
	built into very low-powered devices.

	Stack requirement - just over 4 time size of RSA Public key, so for 2048-bit key that is 1024 bytes
	CPU requirement - does not require multiplication or division for SMALL_AND_SLOW version
	Compiler requirement - minimal C 

    Note that this is a completely standalone module - it calls no MIRACL functions.

	M. Scott April 2015

	To generate a signature using OpenSSL (on Linux)

	(1) Create a text file hello.txt containing just 
	hello world!
	with no return at the end of the file.

	(2) Create a public/private key pair if you don't have one already
	openssl genrsa -out private.pem
	openssl rsa -in private.pem -outform PEM -pubout -out public.pem
	openssl rsa -pubin -text -noout -in public.pem

	By default this creates a 2048 bit key, and uses exponent 65537

	Now cut out the public key bytes generated by that last command, and manually format them (replace : with ,0x etc), and cut and paste into const char public_key[]={}; below

	Now hash and sign the hello.txt file, using sha256
	openssl dgst -sha256 -binary -sign private.pem -out sig hello.txt
	Then hexdump the signature

	hexdump -C sig

	Again manually format, and cut and paste into const char signature[]={}; below

	Works for me! Note one small problem - when reading from a file openssl reads in an extra CR character '0x0a' at the end of the hello.txt file.
	So this '0x0a' character needs to be appended to the text input to this program. See the modification below.

*/

#include <stdio.h>

/* Define this to run quick internal test */

//#define TR_TEST



/*** Architecture/Compiler dependent definitions ***/

#define EXPON 65537
#define REGBITS 32    /* wordlength of computer */
#define RSABITS 2048  /* Must be multiple of wordlength */

/* define one of these */

//#define SMALL_AND_SLOW
#define FAST_BUT_BIGGER

/* a C integer type of CPU Register Size. Can be architecture dependent. */
/* DO NOT be tempted to specify a type greater than CPU wordlength - */
/* that would just be slower and would generate more code */

#if REGBITS == 8
#define REGTYPE char
#define DREGTYPE short
#endif

#if REGBITS == 16
#define REGTYPE short
#define DREGTYPE int
#endif

#if REGBITS == 32
#define REGTYPE int
#define DREGTYPE long long
#endif

#if REGBITS == 64
#define REGTYPE long long
/* if no 128-bit type is available, then SMALL_AND_SLOW is only option */
#endif

/*** end of Architecture/Compiler dependent definitions ***/

/* number of bytes Per CPU Register */
#define REGBYTES (REGBITS/8)
/* RSA Modulus Size as number of computer word */
#define MODSIZE (RSABITS/REGBITS)
typedef unsigned REGTYPE BIG;
#ifdef DREGTYPE
typedef unsigned DREGTYPE DBIG;
#endif
#define RSABYTES (RSABITS/8)

/* SHA256 code */

#define unsign32 unsigned int  /* unsigned 32-bit type */

#define H0 0x6A09E667L
#define H1 0xBB67AE85L
#define H2 0x3C6EF372L
#define H3 0xA54FF53AL
#define H4 0x510E527FL
#define H5 0x9B05688CL
#define H6 0x1F83D9ABL
#define H7 0x5BE0CD19L

typedef struct {
unsign32 length[2];
unsign32 h[8];
unsign32 w[80];
} sha256;

static const unsign32 K[64]={
0x428a2f98L,0x71374491L,0xb5c0fbcfL,0xe9b5dba5L,0x3956c25bL,0x59f111f1L,0x923f82a4L,0xab1c5ed5L,
0xd807aa98L,0x12835b01L,0x243185beL,0x550c7dc3L,0x72be5d74L,0x80deb1feL,0x9bdc06a7L,0xc19bf174L,
0xe49b69c1L,0xefbe4786L,0x0fc19dc6L,0x240ca1ccL,0x2de92c6fL,0x4a7484aaL,0x5cb0a9dcL,0x76f988daL,
0x983e5152L,0xa831c66dL,0xb00327c8L,0xbf597fc7L,0xc6e00bf3L,0xd5a79147L,0x06ca6351L,0x14292967L,
0x27b70a85L,0x2e1b2138L,0x4d2c6dfcL,0x53380d13L,0x650a7354L,0x766a0abbL,0x81c2c92eL,0x92722c85L,
0xa2bfe8a1L,0xa81a664bL,0xc24b8b70L,0xc76c51a3L,0xd192e819L,0xd6990624L,0xf40e3585L,0x106aa070L,
0x19a4c116L,0x1e376c08L,0x2748774cL,0x34b0bcb5L,0x391c0cb3L,0x4ed8aa4aL,0x5b9cca4fL,0x682e6ff3L,
0x748f82eeL,0x78a5636fL,0x84c87814L,0x8cc70208L,0x90befffaL,0xa4506cebL,0xbef9a3f7L,0xc67178f2L};

#define PAD  0x80
#define ZERO 0

/* functions */

#define S(n,x) (((x)>>n) | ((x)<<(32-n)))
#define R(n,x) ((x)>>n)

#define Ch(x,y,z)  ((x&y)^(~(x)&z))
#define Maj(x,y,z) ((x&y)^(x&z)^(y&z))
#define Sig0(x)    (S(2,x)^S(13,x)^S(22,x))
#define Sig1(x)    (S(6,x)^S(11,x)^S(25,x))
#define theta0(x)  (S(7,x)^S(18,x)^R(3,x))
#define theta1(x)  (S(17,x)^S(19,x)^R(10,x))

static void shs_transform(sha256 *sh)
{ /* basic transformation step */
    unsign32 a,b,c,d,e,f,g,h,t1,t2;
    int j;
    for (j=16;j<64;j++) 
        sh->w[j]=theta1(sh->w[j-2])+sh->w[j-7]+theta0(sh->w[j-15])+sh->w[j-16];

    a=sh->h[0]; b=sh->h[1]; c=sh->h[2]; d=sh->h[3]; 
    e=sh->h[4]; f=sh->h[5]; g=sh->h[6]; h=sh->h[7];

    for (j=0;j<64;j++)
    { /* 64 times - mush it up */
        t1=h+Sig1(e)+Ch(e,f,g)+K[j]+sh->w[j];
        t2=Sig0(a)+Maj(a,b,c);
        h=g; g=f; f=e;
        e=d+t1;
        d=c;
        c=b;
        b=a;
        a=t1+t2;        
    }
    sh->h[0]+=a; sh->h[1]+=b; sh->h[2]+=c; sh->h[3]+=d; 
    sh->h[4]+=e; sh->h[5]+=f; sh->h[6]+=g; sh->h[7]+=h; 
} 

void shs256_init(sha256 *sh)
{ /* re-initialise */
    int i;
    for (i=0;i<64;i++) sh->w[i]=0L;
    sh->length[0]=sh->length[1]=0L;
    sh->h[0]=H0;
    sh->h[1]=H1;
    sh->h[2]=H2;
    sh->h[3]=H3;
    sh->h[4]=H4;
    sh->h[5]=H5;
    sh->h[6]=H6;
    sh->h[7]=H7;
}

void shs256_process(sha256 *sh,int byte)
{ /* process the next message byte */
    int cnt;

    cnt=(int)((sh->length[0]/32)%16);
    
    sh->w[cnt]<<=8;
    sh->w[cnt]|=(unsign32)(byte&0xFF);

    sh->length[0]+=8;
    if (sh->length[0]==0L) { sh->length[1]++; sh->length[0]=0L; }
    if ((sh->length[0]%512)==0) shs_transform(sh);
}

void shs256_hash(sha256 *sh,char hash[32])
{ /* pad message and finish - supply digest */
    int i;
    unsign32 len0,len1;
    len0=sh->length[0];
    len1=sh->length[1];
    shs256_process(sh,PAD);
    while ((sh->length[0]%512)!=448) shs256_process(sh,ZERO);
    sh->w[14]=len1;
    sh->w[15]=len0;    
    shs_transform(sh);
    for (i=0;i<32;i++)
    { /* convert to bytes */
        hash[i]=(char)((sh->h[i/4]>>(8*(3-i%4))) & 0xffL);
    }
    shs256_init(sh);
}

/* SHA256 identifier string */
const char SHA256ID[]={0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,0x05,0x00,0x04,0x20};

#ifdef SMALL_AND_SLOW
/* set x=0 */
static void tr_zero(BIG x[])
{
	int i;
	for (i=0;i<MODSIZE;i++) x[i]=0;
}
#endif

/* set x=0 */

static void tr_copy(BIG x[],BIG y[])
{
	int i;
	for (i=0;i<MODSIZE;i++) y[i]=x[i];
}

/* compare x and y. If x>y return 1, if x<y return -1, else return 0 */
static int tr_compare(BIG x[],BIG y[])
{
	int i;
	for (i=MODSIZE-1;i>=0;i--)
	{
		if (x[i]<y[i]) return -1;
		if (x[i]>y[i]) return 1;
	}
	return 0;
}

#ifdef SMALL_AND_SLOW

/* shift Left by 1 bit (multiply by 2) */
static BIG tr_shift(BIG x[])
{
	int i;
	BIG n,c=0;
	for (i=0;i<MODSIZE;i++)
	{
		n=x[i];
		x[i]<<=1; x[i]+=c;
		c=n>>(REGBITS-1);
	}
	return c;
}

/* add x to y */
static BIG tr_add(BIG x[],BIG y[])
{
	int i;
	BIG psum,c=0;
    for (i=0;i<MODSIZE;i++)
    { 
        psum=x[i]+y[i]+c;
        if (psum>x[i]) c=0;
        else if (psum<x[i]) c=1;
        y[i]=psum;
    }
	return c;
}

/* subtract x from y */
static BIG tr_sub(BIG x[],BIG y[])
{
	int i;
	BIG pdiff,b=0;
    for (i=0;i<MODSIZE;i++)
    { 
        pdiff=y[i]-x[i]-b;
        if (pdiff<y[i]) b=0;
        else if (pdiff>y[i]) b=1;
        y[i]=pdiff;
    }
	return b;
}

/* returns i-th bit of x */

static int tr_bit(int i,BIG x[])
{
	int el=i/(REGBITS);
	int b=i%(REGBITS);
	if (x[el]&((BIG)1<<b)) return 1;
	return 0;
}

/* very slow but ultra simple modular multiplication r=a*b mod m */
static void tr_modmul(BIG a[],BIG b[],BIG m[],BIG r[])
{
	int i;
	BIG c;
	tr_zero(r);
	for (i=RSABITS-1;i>=0;i--)
	{
		c=tr_shift(r);
		if (tr_bit(i,b))
		{	
			if (c || tr_compare(r,m)>=0) tr_sub(m,r);
			c=tr_add(a,r);
		}
		if (c || tr_compare(r,m)>=0) tr_sub(m,r);
	}
}

#endif

#ifdef FAST_BUT_BIGGER

static void tr_multiply(BIG x[],BIG y[],BIG z[])
{ /* multiply two big numbers: z=x.y */
    int i,j;
	BIG carry;
    DBIG dble;

    for (i=0;i<2*MODSIZE+1;i++) z[i]=0;

	for (i=0;i<MODSIZE;i++)
    { /* long multiplication */
        carry=0;
        for (j=0;j<MODSIZE;j++)
        { /* multiply each digit of y by x[i] */
            dble=(DBIG)x[i]*y[j]+carry+z[i+j];
            z[i+j]=(BIG)dble;
            carry=(BIG)(dble>>REGBITS);
        }
        z[MODSIZE+i]=carry;
    }
}

static void tr_divide(BIG x[],BIG y[])
{ /* reduce x mod y using division */
    BIG carry,attemp,ldy,sdy,ra,r,tst,psum;
    BIG borrow,dig;
    int i,k,m,w00;
    DBIG dble;

    w00=2*MODSIZE;
    ldy=y[MODSIZE-1];
    sdy=y[MODSIZE-2];
    for (k=w00-1;k>=MODSIZE-1;k--)
    {  /* long division */

        carry=0;
        if (x[k+1]==ldy) /* guess next quotient digit */
        {
            attemp=(BIG)(-1);
            ra=ldy+x[k];
            if (ra<ldy) carry=1;
        }
        else
        {
			dble=((DBIG)x[k+1]<<REGBITS)+x[k];
            attemp=(BIG)(dble/ldy);
            ra=(BIG)(dble-(DBIG)attemp*ldy);
        }

        while (carry==0)
        {
            dble=(DBIG)attemp*sdy;
            r=(BIG)dble;
            tst=(BIG)(dble>>REGBITS);

            if (tst< ra || (tst==ra && r<=x[k-1])) break;
            attemp--;  /* refine guess */
            ra+=ldy;
            if (ra<ldy) carry=1;
        }  
        m=k-MODSIZE+1;
        if (attemp>0)
        { /* do partial subtraction */
            borrow=0;
    
            for (i=0;i<MODSIZE;i++)
            {

                dble=(DBIG)attemp*y[i]+borrow;
                dig=(BIG)dble;
                borrow=(BIG)(dble>>REGBITS);

                if (x[m+i]<dig) borrow++;
                x[m+i]-=dig;
            }
            if (x[k+1]<borrow)
            {  /* whoops! - over did it */
                x[k+1]=0;
                carry=0;
                for (i=0;i<MODSIZE;i++)
                {  /* compensate for error ... */
                    psum=x[m+i]+y[i]+carry;
                    if (psum>y[i]) carry=0;
                    if (psum<y[i]) carry=1;
                    x[m+i]=psum;
                }
                attemp--;  /* ... and adjust guess */
            }
            else x[k+1]-=borrow;
        }
        if (k==w00-1 && attemp==0) w00--;
    }
}

static void tr_modmul(BIG a[],BIG b[],BIG m[],BIG r[])
{
	BIG t[2*MODSIZE+1];
	tr_multiply(a,b,t);
	tr_divide(t,m);
	tr_copy(t,r);
}

#endif

/* force char b into index byte position in x */
static void tr_putbyte(char b,int index,BIG x[])
{
	int el,bp;
	BIG w;
	if (index>=RSABYTES) return;
	el=index/REGBYTES;
	bp=index%REGBYTES;
	w=x[el]&((BIG)0xFF<<(8*bp));
	x[el]^=w;
	x[el]|=((BIG)(unsigned char)b<<(8*bp));
}

/* c=s^EXPON mod m */
static void tr_rsa_pow(BIG m[],BIG s[],BIG c[])
{
	int i;
	BIG t[MODSIZE];
#if EXPON==65537
/* ^65536 */
	tr_modmul(s,s,m,c);  /* square... */
	for (i=0;i<7;i++)
	{
		tr_modmul(c,c,m,t);  /* square... */
		tr_modmul(t,t,m,c);  /* square... */
	}

	tr_modmul(c,c,m,t);  /* square... */
#endif
#if EXPON==3
/* ^2 */
	tr_modmul(s,s,m,t);  /* square... */
#endif
	tr_modmul(s,t,m,c);  /* and multiply */
}

/* Convert from char array to BIG */
static void tr_convert(char *n,BIG pk[])
{
	int i;
	for (i=0;i<RSABYTES;i++)
		tr_putbyte(n[i],RSABYTES-i-1,pk);
}

/* output Number in Hex */
void output(BIG x[])
{
	int i,j;
	for (i=MODSIZE-1;i>=0;i--)
		for (j=REGBYTES-1;j>=0;j--)
			printf("%02x",(unsigned char)(x[i]>>(8*j)));
	printf("\n");
}


void hashit(char *plain,int len,char *h)
{
	int i;
	sha256 sh;
	shs256_init(&sh);
	for (i=0;i<len;i++)
		shs256_process(&sh,plain[i]);

#ifdef TR_TEST
	shs256_process(&sh,0x0a);            /*** append CR from file input ***/
#endif

	shs256_hash(&sh,h);

}

/* PKCS#1 V1.5 padding */
void pkcs_v15(char *h,char m[])
{
	int i;

	m[0]=0;
	m[1]=1;
	for (i=0;i<32;i++) m[RSABYTES+i-32]=h[i];
	for (i=0;i<19;i++) m[RSABYTES+i-51]=SHA256ID[i];
	m[RSABYTES-52]=0;
	for (i=52;i<RSABYTES-2;i++) m[RSABYTES-1-i]=0xff;

}

/* RSA verification - inputs are Message Digest, Public Key, and purported Signature.
   Returns 1 if signature is correct, else 0 
*/

int rsa_verify(char h[],char pub[],char sig[])
{
	BIG c[MODSIZE],n[MODSIZE],s[MODSIZE],d[MODSIZE];
	int i;

/* Convert parameters from char * to BIG format */
	tr_convert(pub,n);
	tr_convert(sig,s);

/* Pad Digest */
//	pkcs_v15(h,p);
//	tr_convert(p,d);

    for (i=0;i<MODSIZE;i++) d[i]=0;
    tr_putbyte(0,RSABYTES-1,d);
    tr_putbyte(1,RSABYTES-2,d);
    for (i=0;i<32;i++) tr_putbyte(h[i],31-i,d);
    for (i=0;i<19;i++) tr_putbyte(SHA256ID[i],32+19-1-i,d);
    tr_putbyte(0,51,d);
    for (i=52;i<RSABYTES-2;i++) tr_putbyte(0xff,i,d);

	tr_rsa_pow(n,s,c);
	if (tr_compare(d,c)==0) return 1;
	return 0;
}


#ifdef TR_TEST

/* test Program */

/* Public Key in ROM, starting with MSB */

const char public_key[]=
{0xb8,0xc9,0x60,0x91,0xf6,0x0d,0x77,0x7d,0x21,0x77,0xe5,0x73,0x01,0x9a,0x4d,0x64,
0xcb,0xc2,0xed,0x83,0x5c,0xdc,0xfe,0x7e,0x40,0xed,0xca,0x7f,0x50,0x3a,0x41,0x06,
0x35,0xec,0x4d,0xd9,0xb7,0xbc,0x31,0xd4,0xc0,0x40,0x1b,0x50,0x4a,0xa1,0x02,0xfd,
0x72,0xcc,0xf1,0x0b,0x25,0xf9,0x15,0xaf,0x55,0xaf,0x2b,0x9b,0xe6,0x50,0xae,0x10,
0xbe,0xdc,0x8d,0xaf,0x0b,0x9d,0x9d,0x18,0xe2,0xb1,0x08,0x03,0x24,0xfa,0x9e,0x2f,
0x27,0xb4,0xf8,0xbb,0xf2,0x41,0x08,0x07,0x4f,0xa6,0xaf,0xe4,0x3e,0x8f,0x3b,0xaf,
0xbd,0x89,0x33,0x50,0x5f,0xfe,0x86,0x99,0xbc,0x36,0xcb,0x2e,0xbb,0x91,0xbb,0x73,
0xfd,0xed,0x0c,0x88,0xfa,0x35,0x22,0x60,0x06,0xc8,0x8b,0x11,0x45,0xed,0xf4,0xb8,
0x5c,0x8a,0xec,0x6d,0xf8,0x2d,0x44,0x63,0x6e,0x5b,0xd2,0x05,0x5c,0xc4,0xee,0xe8,
0x95,0x60,0x8a,0x86,0x54,0xb7,0x78,0xf4,0x9a,0x9d,0xeb,0x2f,0x22,0xb4,0x4f,0x3b,
0x02,0x75,0xb9,0x58,0xa5,0x21,0xac,0x4c,0xb2,0xe9,0x7c,0xb3,0x51,0xe6,0x21,0x93,
0x8b,0xf2,0x20,0x7b,0x95,0xb5,0x1b,0xda,0x88,0x27,0xa4,0x98,0x55,0x22,0x87,0xac,
0xa9,0x24,0x84,0xf5,0x87,0x87,0x52,0x0b,0xdd,0xa8,0xb0,0xcc,0x8e,0x5c,0xcf,0x11,
0x4c,0x0f,0x4a,0x02,0xa6,0x34,0xfc,0x7b,0xed,0x06,0x6d,0x0c,0xdb,0xbb,0xc1,0xb2,
0xe7,0x31,0xfe,0x06,0x82,0xa1,0xc5,0x41,0x35,0x1b,0x5c,0x26,0x14,0x7e,0xbd,0x01,
0xd3,0xdf,0xce,0x39,0xc3,0xc2,0x33,0x65,0x29,0x0a,0x31,0x81,0x9a,0xcf,0xcd,0xc7};

const char signature[]=
{0x0b,0x2c,0x75,0x8b,0x19,0xee,0x91,0x09,0x61,0x7a,0x1b,0xbc,0x5f,0x3d,0x28,0xf9,
0x67,0x23,0x28,0x5f,0x6e,0xed,0x4f,0x7d,0x2d,0x44,0x09,0x83,0x78,0xfe,0x58,0xdf,
0x04,0x1f,0x01,0xe9,0x10,0x9a,0xd7,0x79,0x3a,0x3d,0x64,0x64,0x4c,0xdd,0xef,0x14,
0xbb,0xdd,0xba,0x39,0xe2,0xd1,0x80,0xad,0x03,0xda,0x27,0xec,0x93,0x91,0xe0,0x6b,
0xd9,0x03,0x0b,0x73,0x6e,0xdf,0x8f,0x9e,0x02,0x77,0x51,0xab,0xdf,0x6c,0x0a,0x87,
0x5b,0xb1,0x4a,0x19,0x6a,0xcd,0x1d,0x0d,0x4f,0xde,0x47,0x71,0xef,0x01,0xba,0x18,
0x9e,0xbf,0x54,0xf8,0x4b,0x1d,0x5b,0x33,0xef,0x09,0x8f,0x12,0x47,0x00,0xa1,0x69,
0xac,0x55,0x6c,0x2b,0x11,0x27,0x6e,0x0c,0x60,0x15,0xae,0xf6,0xb7,0x60,0xe5,0x36,
0xaf,0x37,0x7d,0x11,0xed,0x82,0xb6,0x86,0xac,0x9b,0xab,0x6e,0xda,0x87,0x41,0xc6,
0x77,0x21,0x07,0xc6,0xbc,0x41,0x47,0xe1,0x91,0x5f,0xbf,0x7c,0x56,0x90,0x83,0x50,
0x02,0x84,0x7d,0x6f,0x45,0x57,0x74,0xc9,0xe1,0xc7,0xa3,0x81,0x56,0x07,0x42,0x4d,
0x27,0xdf,0x13,0x79,0x4a,0xe3,0xcd,0x4b,0x75,0x0d,0x9d,0x4d,0x22,0x4a,0xc9,0x2d,
0x8d,0x85,0x6f,0x6f,0x0e,0xb8,0x84,0xcb,0xc5,0xcb,0xf9,0x69,0xe8,0xa3,0x91,0xc0,
0xe0,0x45,0xd6,0xd4,0xa5,0xb4,0x0e,0x51,0x24,0x45,0x05,0xf0,0xc7,0x49,0xbc,0xa3,
0xc6,0x76,0x18,0x7f,0x86,0x94,0xc0,0x29,0xac,0xe0,0x33,0x73,0x8f,0x13,0x09,0xe3,
0x94,0xec,0xcc,0xdb,0x37,0x3a,0x01,0xd0,0xe6,0x52,0xc4,0x66,0x48,0xbf,0xcc,0xa4};

int main()
{
	char h[32];

/* hash input h=Sha256(input) */
	hashit("hello world!",12,h);

	if (rsa_verify(h,(char *)public_key,(char *)signature)) printf("Signature is verified\n");
	else printf("Signature is NOT verified\n");

	return 0;
}

#endif