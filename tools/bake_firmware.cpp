#include <QtWidgets/QApplication>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include "ptable.h"
#include "signver.h"

ptable_list* ptable = nullptr;
int dload_id = -1;
int32_t signlen = -1;
char signver_hash[100] = "";
void set_modified() {}
void fdirlist(int) {}
QComboBox* pselector = nullptr;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error(std::string("Cannot open: ") + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
static void replace_code(uint32_t code, const char* path) {
    int idx = -1;
    for (int i = 0; i < ptable->index(); ++i) {
        if (ptable->code(i) == code) { idx = i; break; }
    }
    if (idx < 0) throw std::runtime_error("Required partition code not found: " + std::to_string(code));
    auto data = read_file(path);
    if (data.empty()) throw std::runtime_error(std::string("Empty partition file: ") + path);
    std::cerr << "Replacing code 0x" << std::hex << code << std::dec
              << " (" << ptable->name(idx) << ") with " << data.size() << " bytes\n";
    ptable->replace(idx, data.data(), static_cast<uint32_t>(data.size()));
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc != 5) {
        std::cerr << "Usage: bake_firmware ORIGINAL.bin System.E5785.bin APP.E5785.bin OUTPUT.bin\n";
        return 2;
    }
    try {
        FILE* in = std::fopen(argv[1], "rb");
        if (!in) throw std::runtime_error(std::string("Cannot open original firmware: ") + argv[1]);
        ptable = new ptable_list();
        ptable->findparts(in);
        std::fclose(in);
        if (ptable->index() < 2) throw std::runtime_error("No valid firmware partitions detected");
        const bool compress = ptable->zsize(0) != 0;
        replace_code(0x590000, argv[2]); // System
        replace_code(0x5a0000, argv[3]); // APP
        FILE* out = std::fopen(argv[4], "wb");
        if (!out) throw std::runtime_error(std::string("Cannot create output: ") + argv[4]);
        uint8_t hdr[92] = {};
        hdr[0] = static_cast<uint8_t>(dload_id & 7);
        if (std::fwrite(hdr, 1, sizeof(hdr), out) != sizeof(hdr)) throw std::runtime_error("Failed writing firmware header");
        for (int i = 0; i < ptable->index(); ++i) ptable->save_part(i, out, compress);
        if (std::fclose(out) != 0) throw std::runtime_error("Failed closing output firmware");
        auto original = read_file(argv[1]);
        auto result = read_file(argv[4]);
        if (result.size() < 0x1000 || result.size() == original.size()) {
            std::cerr << "Warning: output size is unexpected; inspect before flashing.\n";
        }
        std::cout << "Created " << argv[4] << " (" << result.size() << " bytes), partitions=" << ptable->index() << "\n";
        delete ptable;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        delete ptable;
        return 1;
    }
}
