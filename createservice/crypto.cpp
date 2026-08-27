#include <openssl/evp.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

int getSHA256(std::string filename, std::string sha256_hex) {
    //std::string filename = filename;
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: cannot open file " << filename << std::endl;
        return 1;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    char buffer[4096];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0)
            EVP_DigestUpdate(ctx, buffer, bytesRead);
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    EVP_DigestFinal_ex(ctx, hash, &length);
    EVP_MD_CTX_free(ctx);

    //std::string hextext;
    std::cout << "SHA 256 of " << filename << ": ";
    for (unsigned int i = 0; i < length; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
            << (int)hash[i];
    }
    std::cout << std::endl;

    return 0;
}

