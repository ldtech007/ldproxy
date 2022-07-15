#ifndef CONT_BASEDES_64_H
#define CONT_BASEDES_64_H

#ifdef __cplusplus
extern "C"{
#endif

int Base64Encode(const unsigned char *pIn, unsigned long uInLen, unsigned char *pOut, unsigned long *uOutLen);

int Base64Decode(const unsigned char *pIn, unsigned long uInLen, unsigned char *pOut, unsigned long *uOutLen);

#ifdef __cplusplus
}
#endif

#endif