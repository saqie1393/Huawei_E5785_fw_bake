// bake_firmware.cpp
//
// Replaces the System and APP partitions inside a Huawei E5785 firmware
// image, using qhuaweiflash's partition-table code.
//
// Usage:
//   bake_firmware ORIGINAL.bin System.E5785.bin APP.E5785.bin OUTPUT.bin

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

// ---------------------------------------------------------------------------
// Globals
//
// NOTE: dload_id is DEFINED in ptable.cpp, so we must NOT redefine it here.
// It is already declared extern in ptable.h.
//
// signlen and signver_hash are normally defined in signver.cpp, which we do
// not compile/link here, so we provide them.
// ---------------------------------------------------------------------------
ptable_list* ptable = nullptr;
int32_t signlen = -1;
char signver_hash[100] = "";

// Stubs for symbols referenced by qhuaweiflash but not needed here.
void set_modified() {}
void fdirlist(int) {}
QComboBox* pselector = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error(std::string("Cannot open: ") + path);
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),
                                std::istreambuf_iterator<char>());
}

// Replace a partition identified by its code (0x590000 = System,
// 0x5a0000 = APP) with the contents of `path`.
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

    // non-const so data.data() yields uint8_t* for
    // ptable_list::replace(int, uint8_t*, uint32_t).
    std::vector<uint8_t> data = read_file(path);
    if (data.empty()) {
        throw std::runtime_error(std::string("Empty partition file: ") + path);
    }
    if (data.size() > UINT32_MAX) {
        throw std::runtime_error(std::string("Partition file is too large: ") + path);
    }

    std::cerr << "Replacing code 0x" << std::hex << code << std::dec
              << " (" << reinterpret_cast<const char*>(ptable->name(idx))
              << ") with " << data.size() << " bytes\n";

    ptable->replace(idx, data.data(), static_cast<uint32_t>(data.size()));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    QApplication app(argc, argv);

    if (argc != 5) {
        std::cerr << "Usage: bake_firmware ORIGINAL.bin System.E5785.bin "
                     "APP.E5785.bin OUTPUT.bin\n";
        return 2;
    }

    try {
        // 1. Load the original firmware's partition table.
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

        // Remember which partitions were compressed in the original image.
        std::vector<bool> was_compressed(ptable->index());
        for (int i = 0; i < ptable->index(); ++i) {
            was_compressed[i] = (ptable->zsize(i) != 0);
        }

        // 2. Swap in the new System and APP images.
        replace_code(0x590000, argv[2]);  // System
        replace_code(0x5a0000, argv[3]);  // APP

        // 3. Write the new firmware image.
        FILE* out = std::fopen(argv[4], "wb");
        if (!out) {
            throw std::runtime_error(std::string("Cannot create output: ") + argv[4]);
        }

        // Copy the original 0x5c-byte file prefix (everything before the
        // first partition header). findparts() skips this prefix.
        {
            FILE* src = std::fopen(argv[1], "rb");
            if (!src) {
                std::fclose(out);
                throw std::runtime_error("Cannot reopen original firmware");
            }
            uint8_t prefix[0x5c];
            if (std::fread(prefix, 1, sizeof(prefix), src) != sizeof(prefix)) {
                std::fclose(src);
                std::fclose(out);
                throw std::runtime_error("Original firmware too short for prefix");
            }
            std::fclose(src);
            if (std::fwrite(prefix, 1, sizeof(prefix), out) != sizeof(prefix)) {
                std::fclose(out);
                throw std::runtime_error("Failed to write output prefix");
            }
        }

        // Write every partition, preserving original compression state.
        for (int i = 0; i < ptable->index(); ++i) {
            ptable->save_part(i, out, was_compressed[i]);
        }

        if (std::fclose(out) != 0) {
            throw std::runtime_error("Failed to finalize output file");
        }

        delete ptable;
        ptable = nullptr;

        std::cerr << "Wrote " << argv[4] << "\n";

    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        if (ptable) {
            delete ptable;
            ptable = nullptr;
        }
        return 1;
    }

    return 0;
}
