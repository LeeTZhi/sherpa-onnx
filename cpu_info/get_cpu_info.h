#ifndef GET_CPU_INFO_H
#define GET_CPU_INFO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SN_RESULT_T (0x01000000)
enum SN_VERIFY_RESULT {
    SN_VERIFY_SUCCESS = SN_RESULT_T & 0x00000000,
    SN_VERIFY_MAC_ERROR = SN_RESULT_T | 0x00000001,
    SN_VERIFY_SERIAL_ERROR = SN_RESULT_T | 0x00000002,
    SN_VERIFY_HASH_ERROR = SN_RESULT_T | 0x00000003,
    SN_VERIFY_SIGNATURE_ERROR = SN_RESULT_T | 0x00000004,
    SN_VERIFY_ERROR = SN_RESULT_T | 0x00000005,
    SN_VERIFY_BUFFER_TOO_SMALL = SN_RESULT_T | 0x00000006,
    SN_VERIFY_INVALID_PARAMETER = SN_RESULT_T | 0x00000007
};

// Function to get the MAC address of the CPU
// Parameters:
//   - address: an array to store the MAC address (6 bytes)
// Returns:
//   - 0 if getting the MAC address is successful, -1 otherwise
int get_mac_address(uint8_t address[6]);

// Function to get the serial number of the CPU
// Parameters:
//   - serial: an array to store the serial number (8 bytes)
// Returns:
//   - 0 if getting the serial number is successful, -1 otherwise
int get_cpu_serial(uint8_t serial[8]);


// Function to calculate the SHA256 hash of the MAC address, serial number, and store it in the hash array
// Parameters:
//   - address: the MAC address (6 bytes)
//   - serial: the serial number (8 bytes)
//   - hash: an array to store the calculated hash (32 bytes)
// Returns:
//   - 0 if calculating the hash is successful, -1 otherwise
int calculate_sha256(uint8_t address[6], uint8_t serial[8], uint8_t hash[32]);


// Function to sign the hash using the private key and store the signature in the signature array
// Parameters:
//   - hash: the hash to be signed
//   - hash_len: the length of the hash
//   - private_key_buffer: the private key used for signing
//   - private_key_buffer_size: the size of the private_key_buffer
//   - signature: an array to store the signature
//   - sig_len: the length of the signature
// Returns:
//   - 0 if signing is successful, -1 otherwise
int sign_hash(
    uint8_t *hash, 
    size_t hash_len, 
    const char *private_key,
    const int private_key_buffer_size,
    uint8_t *signature, 
    size_t *sig_len
    );


// Function to verify the signature of the hash using the public key and the provided signature
// Parameters:
//   - hash: the hash to be verified
//   - hash_len: the length of the hash
//   - public_key: the public key used for verification
//   - public_key_buffer_size: the size of the public_key_buffer
//   - signature: the signature to be verified
//   - sig_len: the length of the signature
// Returns:
//   - 0 if the signature is valid, -1 otherwise
int verify_signature(
    uint8_t *hash, 
    size_t hash_len, 
    const char *public_key, 
    const int public_key_buffer_size,
    uint8_t *signature, 
    size_t sig_len
    );

// Function to generate a key pair and store the private key in the private_key_buffer and the public key in the public_key_buffer
// Parameters:
//   - private_key_buffer: an array to store the private key
//   - private_key_buffer_size: the size of the private_key_buffer
//   - public_key_buffer: an array to store the public key
//   - public_key_buffer_size: the size of the public_key_buffer
// Returns:
//   - 0 if generating the key pair is successful, -1 otherwise
int generate_keypair(
    char *private_key_buffer,
    int private_key_buffer_size,
    char *public_key_buffer, 
    int public_key_buffer_size);

//utlis
// Function to convert a hex string to an array of uint8_t
void hexstr_to_uint8_array(const char *hexstr, uint8_t *out_array, size_t out_array_size);

#endif // GET_CPU_INFO_H