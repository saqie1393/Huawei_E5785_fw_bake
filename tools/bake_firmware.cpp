// bake_firmware.cpp
//
// Replaces the System, APP, and WebUI partitions inside a Huawei E5785 firmware
// image, using qhuaweiflash's partition-table code.
//
// Usage:
//   bake_firmware ORIGINAL.bin System.bin APP.bin WebUI.bin OUTPUT.bin

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
// ---------------------------------------------------------------------------
ptable_list* ptable = nullptr;
int32_t signlen = -1;
char signver_hash[100] = "";

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

// Replace a partition identified by its code with the contents of `path`.
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

    // Ab 6 arguments chahiye (program name + 4 files + output)
    if (argc != 6) {
        std::cerr << "Usage: bake_firmware ORIGINAL.bin System.bin APP.bin WebUI.bin OUTPUT.bin\n";
        return 2;
    }

    try {
        // 1. Load the original firmware's partition table.
        FILE* in = std::fopen(argv[1], "rb");
        if (!in) {
            throw std::runtime_error(std::string("Cannot open original firmware: ") + argv[1]);
        }

        ptable = new ptable_list();
        ptable->findparts(in);
        std::fclose(in);

        if (ptable->index() < 2) {
            throw std::runtime_error("No valid firmware partitions detected");
        }

        // Remember original compression states
        std::vector<bool> was_compressed(ptable->index());
        for (int i = 0; i < ptable->index(); ++i) {
            was_compressed[i] = (ptable->zsize(i) != 0);
        }

        // 2. Swap in the new images (System, APP, WebUI)
        replace_code(0x590000, argv[2]);  // System
        replace_code(0x5a0000, argv[3]);  // APP
        replace_code(0x5b0000, argv[4]);  // WebUI

        // 3. Write the new firmware image.
        FILE* out = std::fopen(argv[5], "wb");
        if (!out) {
            throw std::runtime_error(std::string("Cannot create output: ") + argv[5]);
        }

        // Copy the original 0x5c-byte prefix
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

        // Find indices for modified partitions
        int sys_idx = -1, app_idx = -1, webui_idx = -1;
        for (int i = 0; i < ptable->index(); ++i) {
            if (ptable->code(i) == 0x590000) sys_idx = i;
            if (ptable->code(i) == 0x5a0000) app_idx = i;
            if (ptable->code(i) == 0x5b0000) webui_idx = i;
        }

        // Helper to check if input is already zlib compressed
        auto is_zlib = [](const char* path) -> bool {
            FILE* f = std::fopen(path, "rb");
            if (!f) return false;
            uint8_t hdr[2];
            size_t n = std::fread(hdr, 1, 2, f);
            std::fclose(f);
            return (n == 2 && hdr[0] == 0x78 && (hdr[1] == 0x01 || hdr[1] == 0x9C || hdr[1] == 0xDA));
        };

        // 4. Write every partition with SMART COMPRESSION LOGIC
        for (int i = 0; i < ptable->index(); ++i) {
            bool zflag = was_compressed[i];

            // Apply smart compression only to modified partitions
            if (i == sys_idx || i == app_idx || i == webui_idx) {
                const char* input_path = (i == sys_idx) ? argv[2] : 
                                         (i == app_idx) ? argv[3] : argv[4];
                
                if (!is_zlib(input_path)) {
                    zflag = true;  // Raw input: force compress to keep size normal
                } else {
                    zflag = false; // Already compressed: don't double-compress
                }
            }

            ptable->save_part(i, out, zflag);
        }

        if (std::fclose(out) != 0) {
            throw std::runtime_error("Failed to finalize output file");
        }

        delete ptable;
        ptable = nullptr;

        std::cerr << "Wrote " << argv[5] << "\n";

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
