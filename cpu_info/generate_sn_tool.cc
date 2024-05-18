/*
 * read cpu and mac info and then generate the hash value
 * then save the hash value into a file with hex value
*/

#include <stdio.h>
#include "get_cpu_info.h"

void usage() {
    printf("Usage: ./generate_sn_tool sn.txt\n");
}

int main(int argc, char* argv[]){
    if (argc != 2) {
        usage();
        return -1;
    }

    char* sn_file = argv[1];
    FILE* fp = fopen(sn_file, "w");
    if (fp == NULL) {
        printf("Failed to open %s\n", sn_file);
        return -1;
    }

    uint8_t address[6];
    int ret = get_mac_address(address);
    if (ret != 0) {
        printf("Failed to get mac address\n");
        return -1;
    }

    uint8_t serial[8];
    ret = get_cpu_serial(serial);
    if (ret != 0) {
        printf("Failed to get cpu info\n");
        return -1;
    }

    uint8_t hash[32];
    ret = calculate_sha256(address, serial, hash);
    if (ret != 0) {
        printf("Failed to calculate sn\n");
        return -1;
    }

    //print hex
    for (int i = 0; i < 32; i++) {
        fprintf(fp, "%02x", hash[i]);
    }
    fprintf(fp, "\n");

    fclose(fp);

    return 0;
}