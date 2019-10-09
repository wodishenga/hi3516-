#ifndef SAMPLE_MD5_H
#define SAMPLE_MD5_H

#ifdef __cplusplus
extern "C"
{
#endif


typedef unsigned char           uint8_t;
typedef unsigned short          uint16_t;
typedef unsigned int            uint32_t;

typedef signed char             int8_t;
typedef short                   int16_t;
typedef int                     int32_t;



void md5(const char *initial_msg, size_t initial_len, char *digest);
int md5Test(int argc, char **argv);


#ifdef __cplusplus
}
#endif

#endif

