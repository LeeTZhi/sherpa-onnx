/*
 * 1. load hex sn from file
 * 2. load private key/public key from file
 * 3. sign the sn using the private key, and verify the signature by public key
 * 4. if the signature is valid, then save the sn to file
*/

#include "get_cpu_info.h"

void usage() {
    printf("Usage: ./sign_sn_tool sn.txt private_key.pem public_key.pem signature.txt\n");
}

//buffer should be malloced and size is larger than 1024
int read_pem_file(const char* filename, char* buffer) {
    FILE *file = fopen(filename, "rb");
    
    int length = 0;

    if (file) {
        // 获取文件长度
        fseek(file, 0, SEEK_END);
        length = ftell(file);
        fseek(file, 0, SEEK_SET);

        // 分配内存
        if (buffer) {
            // 读取文件内容
            fread(buffer, 1, length, file);
            buffer[length] = '\0'; // 确保字符串结尾是null字符
        }

        fclose(file);
    }

    return length;
}


int main(int argc, char* argv[]){
    if (argc != 5) {
        usage();
        return -1;
    }

    char* sn_file = argv[1];
    char* private_key_file = argv[2];
    char* public_key_file = argv[3];
    char* signature_file = argv[4];

    FILE* fp = fopen(sn_file, "r");
    if (fp == NULL) {
        printf("Failed to open %s\n", sn_file);
        return -1;
    }

    char sn[65];
    int ret = fscanf(fp, "%s", sn);
    if (ret != 1) {
        printf("Failed to read sn\n");
        return -1;
    }
    fclose(fp);

    // load private key
    char private_key[1024];
    int private_key_len = read_pem_file(private_key_file, private_key);

    // load public key
    char public_key[1024];
    int public_key_len = read_pem_file(public_key_file, public_key);

    // sign the sn
    unsigned char signature[1024]; // 签名空间
    size_t sig_len = 1024;
    uint8_t hash[32];
    hexstr_to_uint8_array(sn, hash, 32);
    ret = sign_hash((uint8_t*)hash, 32, private_key, strlen(private_key)+1, signature, &sig_len);
    if (ret != 0) {
        printf("Failed to sign sn\n");
        return -1;
    }

    // verify the signature
    ret = verify_signature((uint8_t*)hash, 32, public_key, strlen(public_key)+1, signature, sig_len);
    if (ret != 0) {
        printf("Failed to verify signature\n");
        return -1;
    }

    //save the signature to file
    fp = fopen(signature_file, "w");
    if (fp == NULL) {
        printf("Failed to open %s\n", signature_file);
        return -1;
    }
    //save to hex mode
    for (int i = 0; i < sig_len; i++) {
        fprintf(fp, "%02x", signature[i]);
    }
    fclose(fp);

    return 0;
}
