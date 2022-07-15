#include <stdio.h>
#include <stdlib.h>
#include "base64.h"
static const char *g_pCodes =   
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";   
  
static const unsigned char g_pMap[256] =   
{   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,   
     52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255,   
    255, 254, 255, 255, 255,   0,   1,   2,   3,   4,   5,   6,   
      7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,   
     19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,   
    255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,   
     37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,   
     49,  50,  51, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,   
    255, 255, 255, 255   
};

int Base64Encode(const unsigned char *pIn, unsigned long uInLen, unsigned char *pOut, unsigned long *uOutLen)   
{   
    unsigned long i, len2, leven;   
    unsigned char *p;   
  
    if(pIn == NULL || pOut == NULL ||uInLen==0 ||*uOutLen == 0)   
	{
		return -1; 
    }
  
   
  
    len2 = ((uInLen + 2) / 3) << 2;   
    if((*uOutLen) < (len2 + 1)) 
	{
		return -1;  
    }
  
    p = pOut;   
    leven = 3 * (uInLen / 3);   
    for(i = 0; i < leven; i += 3)   
    {   
        *p++ = g_pCodes[pIn[0] >> 2];   
        *p++ = g_pCodes[((pIn[0] & 3) << 4) + (pIn[1] >> 4)];   
        *p++ = g_pCodes[((pIn[1] & 0xf) << 2) + (pIn[2] >> 6)];   
        *p++ = g_pCodes[pIn[2] & 0x3f];   
        pIn += 3;   
    }   
  
    if (i < uInLen)   
    {   
        unsigned char a = pIn[0];   
        unsigned char b = ((i + 1) < uInLen) ? pIn[1] : 0;   
        unsigned char c = 0;   
  
        *p++ = g_pCodes[a >> 2];   
        *p++ = g_pCodes[((a & 3) << 4) + (b >> 4)];   
        *p++ = ((i + 1) < uInLen) ? g_pCodes[((b & 0xf) << 2) + (c >> 6)] : '=';   
        *p++ = '=';   
    }   
  
    *p = 0; // Append NULL byte   
    *uOutLen = p - pOut;   
    return 1;   
}   

int Base64Decode(const unsigned char *pIn, unsigned long uInLen, unsigned char *pOut, unsigned long *uOutLen)
{   
    unsigned long t, x, y, z;   
    unsigned char c;   
    unsigned long g = 3;   
  
    if(pIn == NULL || pOut == NULL ||uInLen==0 ||*uOutLen == 0)   
	{
		return -1; 
    }
  
    for(x = y = z = t = 0; x < uInLen; x++)   
    {   
        c = g_pMap[pIn[x]];   
        if(c == 255) continue;   
        if(c == 254) { c = 0; g--; }   
  
        t = (t << 6) | c;   
  
        if(++y == 4)   
        {   
            if((z + g) > *uOutLen) 
			{ 
				return -1; 
			} // Buffer overflow   
            pOut[z++] = (unsigned char)((t>>16)&255);   
            if(g > 1) pOut[z++] = (unsigned char)((t>>8)&255);   
            if(g > 2) pOut[z++] = (unsigned char)(t&255);   
            y = t = 0;   
        }   
    }   
  
    *uOutLen = z;   
    return 1;   
}  
