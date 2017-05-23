#include <stdint.h>
#include <stdbool.h>

#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_ints.h"
#include "rom_map.h"

#include "hw_memmap.h"
#include "shamd5.h"

#include "MICO.h"

void InitMd5(md5_context *ctx)
{
    MAP_SHAMD5ConfigSet(SHAMD5_BASE, SHAMD5_ALGO_MD5);
        
    return ;
}

void Md5Update(md5_context *ctx, unsigned char *input, int ilen)
{
    // temporary store md5 result into buffer (length of ctx buffer = 16B):
    MAP_SHAMD5DataProcess(SHAMD5_BASE, input, ilen, (uint8_t *)&ctx->digest);
    
    return ;
}

void Md5Final(md5_context *ctx, unsigned char output[16])
{
    memcpy(output, &ctx->digest, 16);
    
    return ;
}
