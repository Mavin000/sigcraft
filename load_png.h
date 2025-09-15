#ifndef SIGCRAFT_LOAD_PNG_H
#define SIGCRAFT_LOAD_PNG_H

#include <string>
#include <optional>
#include <memory>

struct LoadedImage {
    unsigned width, height;
    std::unique_ptr<uint8_t> pixels;
};

std::optional<LoadedImage> load_png(const std::string& path);

#endif
