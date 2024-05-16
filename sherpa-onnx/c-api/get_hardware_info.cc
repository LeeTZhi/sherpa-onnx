#include "get_cpu_info.h"

namespace asr_api {
static const char* pECCPublicKey = "-----BEGIN PUBLIC KEY-----\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEBIzhEDLYtpDFcy2VmCeh5f7Sg5EMGZ6s1k8FEQVGGCMnVtD3DIOnQIm+BrMb+oZ+BZYibPy0lRVuFGBekw2Qag==\n-----END PUBLIC KEY-----";

static int _verify_signature(uint8_t* pSignature, const int sig_len) {
    int ret = 0;
    uint8_t address[6];
    uint8_t serial[8];
    ret = get_mac_address(address);
    if (ret != 0) {
        fprintf(stderr, "Failed to get mac address\n");
        return SN_VERIFY_MAC_ERROR;
    }

    ret = get_cpu_serial(serial);
    if (ret != 0) {
        fprintf(stderr, "Failed to get cpu info\n");
        return SN_VERIFY_SERIAL_ERROR;
    }

    uint8_t hash[32];
    ret = calculate_sha256(address, serial, hash);
    if (ret != 0) {
        fprintf(stderr, "Failed to calculate sn\n");
        return SN_VERIFY_HASH_ERROR;
    }

    ret = verify_signature(hash, 32, pECCPublicKey, strlen(pECCPublicKey)+1, pSignature, sig_len);
    if (ret != 0) {
        fprintf(stderr, "Failed to verify signature\n");
        return SN_VERIFY_SIGNATURE_ERROR;
    }

    return SN_VERIFY_SUCCESS;
}

int verify_authtoken(const char* pSignature, const int sig_len) {
    int ret = 0;
    uint8_t signature[1024];
    size_t signature_len = 1024;
    hexstr_to_uint8_array(pSignature, signature, sig_len);
    signature_len = strlen(pSignature) / 2;

    ret = _verify_signature(signature, signature_len);
    if (ret != 0) {
        fprintf(stderr, "Failed to verify signature\n");
        return ret;
    }
    return SN_VERIFY_SUCCESS;
}

int get_device_id(uint8_t device_id[], int* device_id_len) {
    int ret = 0;
    uint8_t address[6];
    uint8_t serial[8];
    ret = get_mac_address(address);
    if (ret != 0) {
        fprintf(stderr, "Failed to get mac address\n");
        return SN_VERIFY_MAC_ERROR;
    }

    ret = get_cpu_serial(serial);
    if (ret != 0) {
        fprintf(stderr, "Failed to get cpu info\n");
        return SN_VERIFY_SERIAL_ERROR;
    }

    ret = calculate_sha256(address, serial, device_id);
    if (ret != 0) {
        fprintf(stderr, "Failed to calculate sn\n");
        return SN_VERIFY_HASH_ERROR;
    }
    
    *device_id_len = 32;
    return SN_VERIFY_SUCCESS;
}

} // namespace asr_api