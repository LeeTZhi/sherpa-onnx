#include <assert.h>
#include "get_cpu_info.h"

void test_get_mac_address() {
    uint8_t address[6];
    int ret = get_mac_address(address);
    assert(ret == 0);
    // 检查address是否符合预期
    //print hex
    for (int i = 0; i < 6; i++) {
        printf("%02x", address[i]);
    }
    printf("\n");
}

void test_get_cpu_serial() {
    uint8_t serial[8];
    int ret = get_cpu_serial(serial);
    // 检查serial是否符合预期
    //print hex
    for (int i = 0; i < 8; i++) {
        printf("%02x", serial[i]);
    }
    printf("\n");
}

void test_calculate_sha256(const char *private_key_file, const char *public_key_file) {
    // 使用预设的MAC地址和CPU序列号
    uint8_t address[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t serial[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    uint8_t hash[32];
    int ret = calculate_sha256(address, serial, hash);
    // 检查hash是否符合预期
    assert(ret == 0);
    //print hex
    for (int i = 0; i < 32; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");

    // 使用预设的私钥对hash进行签名
    unsigned char signature[1024]; // 签名空间
    size_t sig_len = 1024;

    ret = sign_hash(hash, 32, private_key_file, strlen(private_key_file)+1, signature, &sig_len);
    assert(ret == 0);
    //verify signature
    ret = verify_signature(hash, 32, public_key_file, strlen(public_key_file)+1,signature, sig_len);
    assert(ret == 0);
    printf("Signature verify successfully\n");
    hash[0] ^= 0x01; // 修改hash，使其不再符合预期 
    ret = verify_signature(hash, 32, public_key_file, strlen(public_key_file)+1,signature, sig_len);
    assert(ret != 0);    
}

// 类似地为签名和验证函数编写测试代码

int main() {
    int ret = 0;
    char private_key_file[] = "/tmp/private_key.pem";
    char public_key_file[] = "/tmp/public_key.pem";

    char private_key_buffer[16000], public_key_buffer[16000];
    // 生成密钥对
    ret = generate_keypair(private_key_buffer, 16000,  public_key_buffer, 16000);
    if (ret != 0) {
        printf("Failed to generate key pair\n");
        return ret;
    }
    // 保存密钥对到文件
    FILE *fp = fopen(private_key_file, "w");
    if (!fp) {
        printf("Failed to open %s\n", private_key_file);
        return -1;
    }
    fwrite(private_key_buffer, 1, strlen(private_key_buffer), fp);
    fclose(fp);
    // 保存公钥到文件
    fp = fopen(public_key_file, "w");
    if (!fp) {
        printf("Failed to open %s\n", public_key_file);
        return -1;
    }
    fwrite(public_key_buffer, 1, strlen(public_key_buffer), fp);
    fclose(fp);

    test_get_mac_address();
    test_get_cpu_serial();
    test_calculate_sha256(private_key_buffer, public_key_buffer);
    
    return 0;
}
