#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pk.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include "get_cpu_info.h"

int get_mac_address(uint8_t address[6]) {
    struct ifreq ifr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    ifr.ifr_addr.sa_family = AF_INET;
#ifdef __x86_64__ //NO eth0 on x86, give enp6s0
    strncpy(ifr.ifr_name, "enp6s0", IFNAMSIZ - 1);
#else
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
#endif
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }

    memcpy(address, ifr.ifr_hwaddr.sa_data, 6);
    close(fd);
    return 0;
}

int get_cpu_serial(uint8_t serial[8]) {
#ifdef __x86_64__ //NO cpu id on x86, give the mock serial number
    serial[0] = 0x01;
    serial[1] = 0x23;
    serial[2] = 0x45;
    serial[3] = 0x67;
    serial[4] = 0x89;
    serial[5] = 0xAB;
    serial[6] = 0xCD;
    serial[7] = 0xEF;
    return 0;
#else    
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    char line[256];
    char *serial_str = NULL;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Serial", 6) == 0) {
            serial_str = strchr(line, ':');
            serial_str += 2; // Skip over ": "
            break;
        }
    }

    if (serial_str) {
        for (int i = 0; i < 8; i++) {
            sscanf(serial_str + 2 * i, "%2hhx", &serial[i]);
        }
    } else {
        fprintf(stderr, "Serial number not found\n");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
#endif    
}



int calculate_sha256(uint8_t address[6], uint8_t serial[8], uint8_t hash[32]) {
    mbedtls_sha256_context ctx;
    int ret = 0;
    mbedtls_sha256_init(&ctx);
    ret = mbedtls_sha256_starts(&ctx, 0); // 0 for SHA256, 1 for SHA224
    if (ret != 0) {
        goto EXIT;
    }
    ret = mbedtls_sha256_update(&ctx, address, 6);
    if (ret != 0) {
        goto EXIT;
    }
    ret = mbedtls_sha256_update(&ctx, serial, 8);
    if (ret != 0) {
        goto EXIT;
    }

    mbedtls_sha256_finish(&ctx, hash);

EXIT:
    mbedtls_sha256_free(&ctx);
    return ret;
}


int sign_hash(
    uint8_t *hash, 
    size_t hash_len, 
    const char *private_key, 
    const int private_key_buffer_size, 
    uint8_t *signature, 
    size_t *sig_len
    ) {
    int ret;
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "mbedtls_pk_sign";
    uint8_t signature_buffer[1024];
    size_t  signature_len = 0;
    char error_msg[1024];

    mbedtls_pk_init(&pk);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    // Load private key
    if ((ret = mbedtls_pk_parse_key(&pk, reinterpret_cast<const unsigned char*>(private_key), private_key_buffer_size,
                 NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg)) != 0) {
        mbedtls_strerror(ret, error_msg, 1024);
        fprintf(stderr, "error: %s\n", error_msg);
        goto EXIT;
    }

    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *) pers, strlen(pers))) != 0) {
        mbedtls_strerror(ret, error_msg, 1024);
        fprintf(stderr, "error: %s\n", error_msg);
        goto EXIT;
    }

    if ((ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256,
            hash, hash_len, 
            signature_buffer, 1024, &signature_len,
             mbedtls_ctr_drbg_random, &ctr_drbg)) != 0) {
        mbedtls_strerror(ret, error_msg, 1024);
        fprintf(stderr, "error: %s\n", error_msg);
        goto EXIT;
    }
    if ( *sig_len < signature_len ) {
        ret = SN_VERIFY_BUFFER_TOO_SMALL;
        goto EXIT;
    }

    memcpy(signature, signature_buffer, signature_len);
    *sig_len = signature_len;

EXIT:
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return ret;
}

int verify_signature(
    uint8_t *hash, size_t hash_len, 
    const char *public_key, const int public_key_buffer_size, 
    uint8_t *signature, size_t sig_len
    ) {

    int ret;
    mbedtls_pk_context pk;

    mbedtls_pk_init(&pk);

    // Load public key
    if ((ret = mbedtls_pk_parse_public_key(&pk, reinterpret_cast<const unsigned char*>(public_key), public_key_buffer_size)) != 0) {
        
        goto EXIT;
    }

    if ((ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, hash_len, signature, sig_len)) != 0) {
        goto EXIT;
    }

EXIT:
    mbedtls_pk_free(&pk);

    return ret;
}


// 生成密钥对
int generate_keypair(
    char *private_key_buffer,
    int private_key_buffer_size,
    char *public_key_buffer, 
    int public_key_buffer_size) {
    int ret;
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "mbedtls_ecdsa_genkey";

    mbedtls_pk_init(&pk);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *) pers, strlen(pers));
    if (ret != 0) {
        printf("Failed in mbedtls_ctr_drbg_seed: %d\n", ret);
        return ret;
    }

    ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        printf("Failed in mbedtls_pk_setup: %d\n", ret);
        return ret;
    }

    ret = mbedtls_ecdsa_genkey(mbedtls_pk_ec(pk), MBEDTLS_ECP_DP_SECP256R1, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        printf("Failed in mbedtls_ecdsa_genkey: %d\n", ret);
        return ret;
    }

    ret = mbedtls_pk_write_key_pem(&pk, (unsigned char *)private_key_buffer, private_key_buffer_size);
    if (ret != 0) {
        printf("Failed in mbedtls_pk_write_key_pem: %d\n", ret);
        return ret;
    }

    ret = mbedtls_pk_write_pubkey_pem(&pk, (unsigned char *)public_key_buffer, public_key_buffer_size);
    if (ret != 0) {
        printf("Failed in mbedtls_pk_write_pubkey_pem: %d\n", ret);
        return ret;
    }

    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return 0;
}

void hexstr_to_uint8_array(const char *hexstr, uint8_t *out_array, size_t out_array_size) {
    size_t hexstr_len = strlen(hexstr);

    // 检查输出数组是否足够大
    if (out_array_size < hexstr_len / 2) {
        fprintf(stderr, "Output array is too small.\n");
        return;
    }

    for (size_t i = 0; i < hexstr_len; i += 2) {
        unsigned int byte;
        sscanf(&hexstr[i], "%2x", &byte);
        out_array[i / 2] = (uint8_t)byte;
    }
}
