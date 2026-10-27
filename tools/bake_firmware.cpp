#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "ptable.h"
#include "signver.h"

ptable_list* ptable = nullptr;
int dload_id = -1;
int32_t signlen = -1;
char signver_hash[100] = "";

// Stubs for symbols referenced by qhuaweiflash's partition-table code.
void set_modified() {}
void fdirlist(int) {}
QComboBox* pselector = nullptr;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error(std::string("Cannot open: ") + path);
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),
                                std::istreambuf_iterator<char>());
}

static void replace_code(uint32_t code, const char* path) {
    int idx = -1;
    for (int i = 0; i < ptable->index(); ++i) {
        if (ptable->code(i) == code) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        throw std::runtime_error("Required partition code not found: " +
                                 std::to_string(code));
    }

    // FIX: remove const so data.data() returns uint8_t* (non-const)
    std::vector<uint8_t> data = read_file(path);
    if (data.empty()) {
        throw std::runtime_error(std::string("Empty partition file: ") + path);
    }
    if (data.size() > UINT32_MAX) {
        throw std::runtime_error(std::string("Partition file is too large: ") + path);
    }

    std::cerr << "Replacing code 0x" << std::hex << code << std::dec
              << " (" << ptable->name(idx) << ") with " << data.size()
              << " bytes\n";
    ptable->replace(idx, data.data(), static_cast<uint32_t>(data.size()));
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    if (argc != 5) {
        std::cerr << "Usage: bake_firmware ORIGINAL.bin System.E5785.bin "
                     "APP.E5785.bin OUTPUT.bin\n";
        return 2;
    }

    try {
        FILE* in = std::fopen(argv[1], "rb");
        if (!in) {
            throw std::runtime_error(std::string("Cannot open original firmware: ") +
                                     argv[1]);
        }

        ptable = new ptable_list();
        ptable->findparts(in);
        std::fclose(in);

        if (ptable->index() < 2) {
            throw std::runtime_error("No valid firmware partitions detected");
        }

        const bool compress = ptable->zsize(0) != 0;
        (void)compress;

        replace_code(0x590000, argv[2]);  // System partition
        replace_code(0x5a0000, argv[3]);  // APP partition

        FILE* out = std::fopen(argv[4], "wb");
        if (!out) {
            throw std::runtime_error(std::string("Cannot create output: ") + argv[4]);
        }

        // ============================================================
        // FIX NEEDED HERE:
        // Aapko ptable.h mein dekhna hoga ke pura modified firmware
        // output file mein likhne ke liye kaunsa method hai.
        //
        // Common possibilities:
        //   ptable->save(out);      // agar save(FILE*) exist karta hai
        //   ptable->write(out);     // agar write(FILE*) exist karta hai
        //   ptable->store(out);     // agar store(FILE*) exist karta hai
        //
        // Neeche wali line ko us sahi method se replace karein.
        // Agar method FILE* nahi leta balke path leta hai, to
        // std::fclose(out); hata kar ptable->save(argv[4]); jaisa likhein.
        // ============================================================
        ptable->save(out);   // <-- PLACEHOLDER: ptable.h dekh kar confirm karein

        std::fclose(out);
        delete ptable;
        ptable = nullptr;

    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
