#ifndef LC_COMMON_H
#define LC_COMMON_H

#include <iostream>
#include <regex>
#include <array>
#include <vector>
#include <cstring>
#include "sodium.h"
#include "lc_consts.hpp"
#include "lc_keymgr.hpp"
#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>

namespace lc_utils {

    static uint64_t hash_client_info (const std::array<uint8_t, 
        CID_BYTES>& client_cid, 
        const std::array<uint8_t, crypto_box_PUBLICKEYBYTES>& 
        client_public_key) {
        
        uint8_t hash[CIF_BYTES];
        std::array<uint8_t, CID_BYTES + crypto_box_PUBLICKEYBYTES> client_info;
        std::copy(client_cid.begin(), client_cid.end(), client_info.begin());
        std::copy(client_public_key.begin(), client_public_key.end(), 
                  client_info.begin() + CID_BYTES);
        crypto_generichash(hash, CIF_BYTES, client_info.data(), 
                           client_info.size(), nullptr, 0);
        uint64_t ret = 0;
        for(uint8_t i = 0; i < CIF_BYTES; ++ i)
            ret |= (static_cast<uint64_t>(hash[i]) << (i << 3));
        return ret;
    }

    static std::vector<std::string> split_buffer_by_null (const uint8_t *data, 
        const size_t data_bytes, const size_t max_items) {
        
        std::vector<std::string> ret;
        const uint8_t *start = data;
        const uint8_t *end = data + data_bytes;
        const uint8_t *current = start;
        while (current < end && ret.size() <= max_items) {
            auto next_null = static_cast<const uint8_t *>
                             (std::memchr(current, 0x00, (end - current)));
            if (next_null != nullptr) {
                size_t length = next_null - current;
                ret.emplace_back(reinterpret_cast<const char *>(current), 
                                 length);
                current = next_null + 1;
            }
            else {
                size_t length = end - current;
                if (length > 0) 
                    ret.emplace_back(reinterpret_cast<const char *>(current), 
                                     length);
                break;
            }
        }
        return ret;
    }

    static std::array<uint8_t, 8> u64_to_bytes (uint64_t num) {
        std::array<uint8_t, 8> ret;
        for(uint8_t i = 0; i < 8; ++ i) 
            ret[i] = static_cast<uint8_t>((num >> (i << 3)) & 0xFF);
        return ret;
    }

    static uint64_t bytes_to_u64 (std::array<uint8_t, 8> arr) {
        uint64_t num = 0;
        for(uint8_t i = 0; i < 8; ++ i)
            num |= (static_cast<uint64_t>(arr[i]) << (i << 3));
        return num;
    }

    static void generate_aes_nonce (
        std::array<uint8_t, crypto_aead_aes256gcm_NPUBBYTES>& aes256gcm_nonce) {
        
        randombytes_buf(aes256gcm_nonce.data(), aes256gcm_nonce.size());
    }

    // Only Alphabet, numbers, and special chars are allowed.
    // Length: 8-64
    static int pass_fmt_check (const std::string& pass_str) {
        if (pass_str.size() < PASSWORD_MIN_BYTES || 
            pass_str.size() > PASSWORD_MAX_BYTES)
            return -1; // Length error.

        uint8_t num = 0, lower = 0, upper = 0, special = 0;
        for(auto c : pass_str) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
                num = 1; continue;
            }
            if (std::islower(static_cast<unsigned char>(c))) {
                lower = 1; continue;
            }
            if (std::isupper(static_cast<unsigned char>(c))) {
                upper = 1; continue;
            }
            if (std::find(special_chars.begin(), special_chars.end(), c) != 
                special_chars.end()) {
                special = 1; continue;
            }
            return 1; // Illegal char found.
        }
        if ((num + special + lower + upper < 3) || (special == 0))
            return 2; // Not complex enough.
        return 0; // Good to go.
    }

    // Only Alphabet, numbers, and hyphen are allowed.
    // Length: 4-64
    static int user_name_fmt_check (const std::string& uname) {
        if (uname.size() < ULOGIN_MIN_BYTES || uname.size() > UNAME_MAX_BYTES)
            return -1; // Length error.
        for(auto c : uname) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && 
                c != '_')
                return 1; // Illegal char found.
        }
        return 0; // Good to go.
    }

    static bool pass_hash (std::string& password, 
        std::array<char, crypto_pwhash_STRBYTES>& hashed_pwd) {
        
        auto ret = 
        crypto_pwhash_str(
            hashed_pwd.data(), 
            password.c_str(), 
            password.size(), 
            crypto_pwhash_OPSLIMIT_INTERACTIVE, 
            crypto_pwhash_MEMLIMIT_INTERACTIVE
        );
        password.clear(); // For security reasons, we clean the string after hashing.
        if (ret == 0)
            return true;
        return false;
    }

    static int email_fmt_check (const std::string& email) {
        if (email.empty() || email.size() > UEMAIL_MAX_BYTES)
            return -1;
        std::regex email_regex
            (R"(^[a-zA-Z0-9_.+-]+@(?:[a-z0-9-]+\.)+[a-zA-Z0-9-]{2,}$)");
        if (!std::regex_match(email, email_regex)) 
            return 1;
        return 0;
    }

    static int calc_aes_key (
        std::array<uint8_t, crypto_aead_aes256gcm_KEYBYTES>& aes_key, 
        const std::array<uint8_t, crypto_box_PUBLICKEYBYTES>& pk, 
        const std::array<uint8_t, crypto_box_SECRETKEYBYTES>& sk) {
        
        return crypto_box_beforenm(aes_key.data(), pk.data(), sk.data());
    }

    static size_t calc_encrypted_len(const size_t raw_bytes) {
        return (1 + crypto_aead_aes256gcm_NPUBBYTES + SID_BYTES + CIF_BYTES + 
                raw_bytes + crypto_aead_aes256gcm_ABYTES);
    }

     static bool string_to_u16(const std::string& str, uint16_t& res) {
        if (str.size() > 5)
            return false;
        for(auto c : str) {
            if (!isdigit(c))
                return false;
        }
        unsigned long n = std::stoul(str);
        if (n > std::numeric_limits<uint16_t>::max())
            return false;
        res = static_cast<uint16_t>(n);
        return true;
    }

    static bool get_addr_info(std::string& addr_str, 
        std::array<char, INET_ADDRSTRLEN>& first_ipv4_addr) {
        
        if (addr_str.empty())
            return false;
        struct addrinfo hints, *res = nullptr;
        std::memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        std::memset(first_ipv4_addr.data(), 0, first_ipv4_addr.size());
        auto status = getaddrinfo(addr_str.c_str(), nullptr, &hints, &res);
        if (status != 0)
            return false;
        struct sockaddr_in *first = (sockaddr_in *)res->ai_addr;
        inet_ntop(AF_INET, &(first->sin_addr), first_ipv4_addr.data(), 
                    first_ipv4_addr.size());
        freeaddrinfo(res);
        return true;
    }

    bool sign_crypto_pk(const key_mgr_25519& key_mgr, 
        std::array<uint8_t, 
        crypto_sign_BYTES + crypto_box_PUBLICKEYBYTES>& signed_cpk) {
        
        if (!key_mgr.is_activated())
            return false;
        auto sign_pk = key_mgr.get_sign_pk();
        auto crypto_pk = key_mgr.get_crypto_pk();
        auto sign_sk = key_mgr.get_sign_sk();
        unsigned long long signed_len;
        if (crypto_sign(signed_cpk.data(), &signed_len, crypto_pk.data(), 
            crypto_pk.size(), sign_sk.data()) != 0)
            return false;

        return true;
    }

    void print_array(const uint8_t *arr, const size_t n) {
        printf("\n");
        for(size_t i = 0; i < n; ++ i) 
            printf("%x ", arr[i]);
        printf("\n %lu \n", n);
    }
}

#endif