// adapted (stolen) from Rodent
#include "load_png.h"

#include "png.h"

#include <iostream>
#include <fstream>

static void read_from_stream(png_structp png_ptr, png_bytep data, png_size_t length) {
    png_voidp a = png_get_io_ptr(png_ptr);
    ((std::istream*)a)->read((char*)data, length);
}

std::optional<LoadedImage> load_png(const std::string& path) {
    std::ifstream file(path, std::ifstream::binary);
    if (!file) {
        fprintf(stderr, "load_png: %s not found\n", path.c_str());
        return std::nullopt;
    }

    // Read signature
    char sig[8];
    file.read(sig, 8);
    if (!png_check_sig((unsigned char*)sig, 8))
        return std::nullopt;

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr)
        return std::nullopt;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return std::nullopt;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return std::nullopt;
    }

    png_set_sig_bytes(png_ptr, 8);
    png_set_read_fn(png_ptr, (png_voidp)&file, read_from_stream);
    png_read_info(png_ptr, info_ptr);

    LoadedImage img;

    img.width    = png_get_image_width(png_ptr, info_ptr);
    img.height   = png_get_image_height(png_ptr, info_ptr);

    png_uint_32 color_type = png_get_color_type(png_ptr, info_ptr);
    png_uint_32 bit_depth  = png_get_bit_depth(png_ptr, info_ptr);

    // Expand paletted and grayscale images to RGB
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    } else if (color_type == PNG_COLOR_TYPE_GRAY ||
               color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }

    // Transform to 8 bit per channel
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    // Get alpha channel when there is one
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

        // Otherwise add an opaque alpha channel
    else
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

    img.pixels.reset(new uint8_t[4 * img.width * img.height]);
    std::unique_ptr<png_byte[]> row_bytes(new png_byte[img.width * 4]);
    for (size_t y = 0; y < img.height; y++) {
        png_read_row(png_ptr, row_bytes.get(), nullptr);
        uint8_t* img_row = img.pixels.get() + 4 * img.width * (img.height - 1 - y);
        for (size_t x = 0; x < img.width; x++) {
            for (size_t c = 0; c < 4; ++c)
                img_row[x * 4 + c] = row_bytes[x * 4 + c];
        }
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    //gamma_correct(img);
    return std::make_optional<LoadedImage>(std::move(img));
}