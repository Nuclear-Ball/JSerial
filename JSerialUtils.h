#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

namespace JSerial {
    namespace Utilities {
        template<typename T> inline std::string MemblockToString(T *memory, size_t size) {
            return {reinterpret_cast<const char *>(memory), sizeof(T) * size};
        }

        inline void OverrideString(std::string *s1, const std::string &s2, size_t pos) {
            memcpy(&s1->operator[](pos), s2.data(), s2.size());
        }

        std::string ReadFile(const std::string &path) {
            std::ifstream file(path);

            file.seekg(0, std::ios::end);
            const size_t filesize = file.tellg();
            file.seekg(0, std::ios::beg);

            constexpr size_t invalid_filesize = -1ull;
            if (filesize == invalid_filesize) {
                std::cerr << "ERROR: file was not successfully opened\n";
                return "";
            }

            std::string res(filesize, 0);

            file.read(const_cast<char *>(res.data()), filesize);
            file.close();

            return res;
        }

        void SaveFile(const std::string &path, const std::string &data) {
            std::ofstream file(path);
            file.write(data.c_str(), data.size());
            file.close();
        }
    }

    namespace Bits {
        template<typename T> inline std::string Separate(T dat)
            { return {reinterpret_cast<const char*>(&dat), sizeof(T)}; }
        template<typename T> inline T Merge(const std::string& dat)
            { return *reinterpret_cast<const T*>(dat.data()); }
    }
}