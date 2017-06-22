#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include        <ctype.h>

//#define OLD_DES
/* #ifndef USE_CRYPT */
//#define OPENSSL_DES_LIBDES_COMPATIBILITY
//#include <openssl/opensslconf.h>
#include <openssl/des.h>	/* more */
//#include <openssl/rand.h>
//#include <openssl/ui_compat.h>

//#include <des.h>		/* descrypt */
/* #endif */

/* #include        "des.h" */
#include        "md4.h"
#include        "md5.h"
//#include        <md5.h>
#include        <sha.h>
#include	"radiusd.h"

static const char *letters = "0123456789ABCDEF";
int hex2bin(const char *szHex, unsigned char *szBin, int len){
	char *c1, *c2;
	int i;

	for (i = 0; i < len; i++){
		if( !(c1 = memchr(letters, toupper((int)szHex[i<<1]), 16)) ||
		    !(c2 = memchr(letters, toupper((int)szHex[(i<<1)+1]), 16)))
		break;
		szBin[i] = ((c1 - letters)<<4) + (c2 - letters);
	}
	return i;
}

static u_char
Get7Bits(u_char *input, int startBit){
    register unsigned int       word;
    word  = (unsigned)input[startBit / 8] << 8;
    word |= (unsigned)input[startBit / 8 + 1];
    word >>= 15 - (startBit % 8 + 7);
    return word & 0xFE;
}

static 
void MakeKey(u_char *key,	 /* IN  56 bit DES key missing parity bits */
	     u_char *des_key)	/* OUT 64 bit DES key with parity bits added */
{
    des_key[0] = Get7Bits(key,  0);
    des_key[1] = Get7Bits(key,  7);
    des_key[2] = Get7Bits(key, 14);
    des_key[3] = Get7Bits(key, 21);
    des_key[4] = Get7Bits(key, 28);
    des_key[5] = Get7Bits(key, 35);
    des_key[6] = Get7Bits(key, 42);
    des_key[7] = Get7Bits(key, 49);
#ifndef USE_CRYPT
#ifndef OLD_DES
    DES_set_odd_parity((DES_cblock *)des_key);
#else
    DES_set_odd_parity((des_cblock *)des_key);
#endif
#endif
}

#ifdef USE_CRYPT
/* in == 8-byte string (expanded version of the 56-bit key)
 * out == 64-byte string where each byte is either 1 or 0
 * Note that the low-order "bit" is always ignored by by setkey()
 */
static void Expand(u_char *in, u_char *out){
        int j, c;
        int i;
        for(i = 0; i < 64; in++){
                c = *in;
                for(j = 7; j >= 0; j--)
                        *out++ = (c >> j) & 01;
                i += 8;
        }
}

/* The inverse of Expand
 */
static void Collapse(u_char *in, u_char *out){
        int j;
        int i;
        unsigned int c;
        for (i = 0; i < 64; i += 8, out++){
            c = 0;
            for (j = 7; j >= 0; j--, in++)
                c |= *in << j;
            *out = c & 0xff;
        }
}

void
DesEncrypt(clear, key, cipher)
    u_char *clear;      /* IN  8 octets */
    u_char *key;        /* IN  7 octets */
    u_char *cipher;     /* OUT 8 octets */
{
    u_char des_key[8];
    u_char crypt_key[66];
    u_char des_input[66];

    MakeKey(key, des_key);
    Expand(des_key, crypt_key);
    setkey(crypt_key);
    Expand(clear, des_input);
    encrypt(des_input, 0);
    Collapse(des_input, cipher);
}
#else /* USE_CRYPT */
void
DesEncrypt(u_char *clear,      /* IN  8 octets */
	   u_char *key,        /* IN  7 octets */
	   u_char *cipher)     /* OUT 8 octets */
{
#ifndef OLD_DES
	DES_cblock		des_key;
	DES_key_schedule	key_schedule;
#else
	des_cblock          des_key; 
	des_key_schedule    key_schedule; 
#endif

    MakeKey(key, des_key);
/* new int DES_set_key(const_DES_cblock *key,DES_key_schedule *schedule); */
#ifndef OLD_DES
    DES_set_key(&des_key, &key_schedule);
#else
    DES_set_key(&des_key, key_schedule);
#endif
/* new
void DES_ecb_encrypt(const_DES_cblock *input,DES_cblock *output,
                     DES_key_schedule *ks,int enc);
*/
#ifndef OLD_DES
    DES_ecb_encrypt((const_DES_cblock *)clear, (DES_cblock *)cipher, &key_schedule, 1);
#else
    DES_ecb_encrypt((des_cblock *)clear, (des_cblock *)cipher, key_schedule, 1);
#endif
}
#endif /* USE_CRYPT */

/*
 *      ntpwdhash converts Unicode password to 16-byte NT hash
 *      with MD4
 */
void ntpwdhash(u_char *hash, const u_char *passwd){
        u_char upass[513];
        int nlen;
        int i;

	memset(upass, 0, sizeof(upass));
        nlen = strlen(passwd);
        for (i = 0; i < nlen; i++){
                upass[i * 2] = (u_char)passwd[i];
        }

        md4_calc(hash, upass, nlen * 2); 
}

/*
 *      lmpwdhash converts 14-byte null-padded uppercase OEM
 *      password to 16-byte DES hash with predefined salt string
 */
void lmpwdhash(char *hash, const u_char *szPassword){
        u_char up[14];
        u_char clear[] = "KGS!@#$%";
        int i;
        memset(up, 0, sizeof(up));
        /*for (i = 0; i < 14 && szPassword[i]; i++) */
        for (i = 0; i < strlen(szPassword); i++)
                up[i] = (u_char)toupper(szPassword[i]);
        /* Obtain DES hash of OEM password */
        DesEncrypt(clear, up + 0, hash + 0);
        DesEncrypt(clear, up + 7, hash + 8); 
}

/*
 *      mschap takes an 8-byte challenge string and SMB password
 *      and returns a 24-byte response string in szResponse
 */
void mschap(const char *Challenge, u_char *passwd,
					char *Response, int UseNT){

        char MD4Hash[21];
	char hash[16];

	if (passwdhash == 0){
		if (UseNT)
			ntpwdhash(hash, passwd);
		else
			lmpwdhash(hash, passwd);
	} else {
		if (UseNT)
			hex2bin(passwd, hash, 16);
		else
			{}
	}

        /* initialize hash string */
	memset(MD4Hash, 0, sizeof(MD4Hash));
        memcpy(MD4Hash, hash, 16);
	memset(Response, 0, sizeof(Response));

        /*
         *
         *      challenge_response takes an 8-byte challenge string and a
         *      21-byte hash (16-byte hash padded to 21 bytes with zeros) and
         *      returns a 24-byte response in szResponse
         */
		
        DesEncrypt((char *)Challenge, MD4Hash +  0, Response +  0);
        DesEncrypt((char *)Challenge, MD4Hash +  7, Response +  8);
        DesEncrypt((char *)Challenge, MD4Hash + 14, Response + 16);
}


/*
 *      challenge_hash() is used by mschap2() and auth_response()
 *      implements RFC2759 ChallengeHash()
 *      generates 64 bit challenge
 */
void challenge_hash(const char *peer_challenge,
		    const char *auth_challenge,
                    const char *username,
		    char *challenge)
{
        u_char hash[20];
        SHA1_CTX Context;
	char	*user;

    /* remove domain from "domain\username" */
    if ((user = strrchr(username, '\\')) != NULL)
        ++user;
    else
        user = username;

        SHA1_Init(&Context);
        SHA1_Update(&Context, peer_challenge, 16);
        SHA1_Update(&Context, auth_challenge, 16);
        SHA1_Update(&Context, user, strlen(user));
        SHA1_Final(hash, &Context);
        memcpy(challenge, hash, 8);
}

void mschap2(const char *peer_challenge,
	     const char *auth_challenge,
	     SQL_PWD *spwd, 
	     char *response)
{
        char challenge[8];
        challenge_hash(peer_challenge, auth_challenge, spwd->fname,
                challenge);
        mschap(challenge, spwd->passwd, response, 1);
}

/*
 *      auth_response() generates MS-CHAP v2 SUCCESS response
 *      according to RFC 2759 GenerateAuthenticatorResponse()
 *      returns 42-octet response string
 */
void mschap2_resp(SQL_PWD *spwd, 
		char *ntresponse,
                char *peer_challenge, 
		char *auth_challenge,
                char *response)
{
        SHA1_CTX Context;
        char hashhash[16];
        u_char magic1[39] =
               {0x4D, 0x61, 0x67, 0x69, 0x63, 0x20, 0x73, 0x65, 0x72, 0x76,
                0x65, 0x72, 0x20, 0x74, 0x6F, 0x20, 0x63, 0x6C, 0x69, 0x65,
                0x6E, 0x74, 0x20, 0x73, 0x69, 0x67, 0x6E, 0x69, 0x6E, 0x67,
                0x20, 0x63, 0x6F, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x74};

        u_char magic2[41] =
               {0x50, 0x61, 0x64, 0x20, 0x74, 0x6F, 0x20, 0x6D, 0x61, 0x6B,
                0x65, 0x20, 0x69, 0x74, 0x20, 0x64, 0x6F, 0x20, 0x6D, 0x6F,
                0x72, 0x65, 0x20, 0x74, 0x68, 0x61, 0x6E, 0x20, 0x6F, 0x6E,
                0x65, 0x20, 0x69, 0x74, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6F,
                0x6E};
        u_char challenge[8];
        u_char digest[20];
	int	i; 
        /*
         * Hash password hash into hashhash
         */
	if (passwdhash == 0)
        	ntpwdhash(hashhash, spwd->passwd);
	else	hex2bin(spwd->passwd, hashhash, 16);

        md4_calc(hashhash, hashhash, 16);

        SHA1_Init(&Context);
        SHA1_Update(&Context, hashhash, 16);
        SHA1_Update(&Context, ntresponse, 24);
        SHA1_Update(&Context, magic1, sizeof(magic1) /* 39 */);
        SHA1_Final(digest, &Context);

        challenge_hash(peer_challenge, auth_challenge, spwd->fname, challenge);

        SHA1_Init(&Context);
        SHA1_Update(&Context, digest, 20);
        SHA1_Update(&Context, challenge, 8);
        SHA1_Update(&Context, magic2, sizeof(magic2) /* 41 */);
        SHA1_Final(digest, &Context);
        /*
         * Encode the value of 'Digest' as "S=" followed by
         * 40 ASCII hexadecimal digits and return it in
         * AuthenticatorResponse.
         * For example,
         *   "S=0123456789ABCDEF0123456789ABCDEF01234567"
         */
        response[0] = 'S';
        response[1] = '=';
	for (i = 0; i < 20; i++)
		sprintf(&response[i*2+2], "%02X", digest[i]);
}


static void add_reply(){
}

static void mppe_add_reply(){
}

static u_char SHSpad1[40] =
               { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,};

static u_char SHSpad2[40] =
               { 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,};

static u_char magic1[27] =
               { 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74,
                 0x68, 0x65, 0x20, 0x4d, 0x50, 0x50, 0x45, 0x20, 0x4d,
                 0x61, 0x73, 0x74, 0x65, 0x72, 0x20, 0x4b, 0x65, 0x79 };

static u_char magic2[84] =
               { 0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
                 0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
                 0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
                 0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20, 0x6b, 0x65, 0x79,
                 0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73,
                 0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73, 0x69, 0x64, 0x65,
                 0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
                 0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
                 0x6b, 0x65, 0x79, 0x2e };

static u_char magic3[84] =
               { 0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
                 0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
                 0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
                 0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
                 0x6b, 0x65, 0x79, 0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68,
                 0x65, 0x20, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73,
                 0x69, 0x64, 0x65, 0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73,
                 0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20,
                 0x6b, 0x65, 0x79, 0x2e };

void mppe_GetMasterKey(u_char *nt_hashhash,
		       u_char *nt_response,
                       u_char *masterkey)
{
       u_char digest[20];
       SHA1_CTX Context;

       memset(digest, 0, 20);
       SHA1_Init(&Context);
       SHA1_Update(&Context,nt_hashhash,16);
       SHA1_Update(&Context,nt_response,24);
       SHA1_Update(&Context,magic1, sizeof(magic1) /*27*/);
       SHA1_Final(digest,&Context);
       memcpy(masterkey,digest,16);
}

void mppe_GetAsymmetricStartKey(u_char *masterkey,
				u_char *sesskey,
                                int keylen,
				int issend)
{
       u_char digest[20];
       u_char *s;
       int k;
       SHA1_CTX Context;
       memset(digest, 0, 20);
       if (issend){
               s = magic3;
       } else {
               s = magic2;
       }
       SHA1_Init(&Context);
       SHA1_Update(&Context,masterkey,16);
        for (k = 0; k < 4; k++)
       		SHA1_Update(&Context,SHSpad1,10);
       SHA1_Update(&Context,s,84);

        for (k = 0; k < 4; k++)
       		SHA1_Update(&Context,SHSpad2,10);
       SHA1_Final(digest,&Context);
       memcpy(sesskey,digest,keylen);
}


void mppe_chap2_get_keys128(u_char *nt_hashhash,
			    u_char *nt_response,
                            u_char *sendkey,
			    u_char *recvkey)
{
       u_char masterkey[16];

       mppe_GetMasterKey(nt_hashhash,nt_response,masterkey);
       mppe_GetAsymmetricStartKey(masterkey,sendkey,16,1);
       mppe_GetAsymmetricStartKey(masterkey,recvkey,16,0);
}

/*      Not requiered, because encoding will be performed by
        tunnel_pwencode */
void mppe_gen_respkey(u_char *secret,
		      u_char *vector,
                      u_char *salt,
		      u_char *enckey,
		      u_char *key)
{
       u_char plain[32];
       u_char buf[16];
       int i;
       MD5_CTX Context;
       int slen = strlen(secret);
       memset(key,0,34);

       memset(plain,0,32);
       plain[0] = 16;
       memcpy(plain + 1,enckey,16);

       MD5Init(&Context);
       MD5Update(&Context,secret,slen);
       MD5Update(&Context,vector,AUTH_VECTOR_LEN);
       MD5Update(&Context,salt,2);
       MD5Final(buf,&Context);
       for(i=0;i < 16;i++) {
               plain[i] ^= buf[i];
       }

       MD5Init(&Context);
       MD5Update(&Context,secret,slen);
       MD5Update(&Context,plain,16);
       MD5Final(buf,&Context);
       for(i=0;i < 16;i++) {
               plain[i + 16] ^= buf[i];
       }
       memcpy(key,salt,2);
       memcpy(key + 2,plain,32);
}


void mppe_chap2_gen_keys128(u_char *secret, u_char *vector,
                            u_char *nt_hash,u_char *response,
                            u_char *sendkey,u_char *recvkey)
{
        u_char enckey1[16];
        u_char enckey2[16];
        u_char salt[2];
        u_char nt_hashhash[16];

	if (passwdhash == 0)
        	ntpwdhash(nt_hashhash, nt_hash);
	else    hex2bin(nt_hash, nt_hashhash, 16);

        md4_calc(nt_hashhash, nt_hashhash, 16);
			/* --> enckey1,2 */
        mppe_chap2_get_keys128(nt_hashhash,response,enckey1,enckey2);
#if 1
        salt[0] = (vector[0] ^ vector[1] ^ 0x3A) | 0x80;
        salt[1] = (vector[2] ^ vector[3] ^ vector[4]);
	/* --> secret,vector,salt,enckey1  : sendkey --> */
        mppe_gen_respkey(secret,vector,salt,enckey1,sendkey);

        salt[0] = (vector[0] ^ vector[1] ^ 0x4e) | 0x80;
        salt[1] = (vector[5] ^ vector[6] ^ vector[7]);
	/* --> secret,vector,salt,enckey2  : recvkey --> */
        mppe_gen_respkey(secret,vector,salt,enckey2,recvkey);
#else
	/* dictionary.microsoft defines these attributes as
	 * 'encrypt=2'.  The functions in src/lib/radius.c will
	 * take care of encrypting/decrypting them as appropriate,
	 * so that we don't have to.
	 */
	/* send/recv <-- enckey1/2 */
        memcpy(sendkey, enckey1, 16);
        memcpy(recvkey, enckey2, 16); 
#endif
}
