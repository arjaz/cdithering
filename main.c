#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

typedef struct { uint8_t x, y, z; } uint8x3;
typedef struct { int32_t x, y, z; } int32x3;

uint8x3 read_uint8x3(uint8_t *data) {
    return (uint8x3){ data[0], data[1], data[2] };
}

uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

uint8x3 palette[] = {
    {0,   0,   0},
    {255, 255, 255},
};
uint32_t palette_length = 2;

// Pad with 1 black pixel
uint8_t *pad_image(const uint8_t *img_rgb, int width, int height, int channels) {
    int width_padded = width + 2, height_padded = height + 2;
    uint8_t *img_padded = calloc(width_padded * height_padded * channels, 1);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            for (int ch = 0; ch < channels; ch++) {
                img_padded[((y + 1) * width_padded + (x + 1)) * channels + ch] =
                    img_rgb[(y * width + x) * channels + ch];
            }
        }
    }
    return img_padded;
}

uint8x3 nearest_color(uint8x3 c, uint8x3 *palette, uint32_t len) {
    uint8x3 best = palette[0];
    int32_t best_d = INT32_MAX;

    for (uint32_t i = 0; i < len; i++) {
        int32_t dx = (int32_t)c.x - palette[i].x;
        int32_t dy = (int32_t)c.y - palette[i].y;
        int32_t dz = (int32_t)c.z - palette[i].z;
        int32_t d = dx*dx + dy*dy + dz*dz;
        if (d < best_d) {
            best_d = d;
            best = palette[i];
        }
    }
    return best;
}

void dither(int width, int height, uint8_t *img) {
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int c = 3;

            uint8x3 old_pixel = read_uint8x3(img + (y * width + x) * c);
            uint8x3 new_pixel = nearest_color(old_pixel, palette, palette_length);

            int32x3 err = (int32x3){
                (int32_t)old_pixel.x - new_pixel.x,
                (int32_t)old_pixel.y - new_pixel.y,
                (int32_t)old_pixel.z - new_pixel.z,
            };

            img[(y * width + x) * c + 0] = new_pixel.x;
            img[(y * width + x) * c + 1] = new_pixel.y;
            img[(y * width + x) * c + 2] = new_pixel.z;

            img[(y * width + x + 1) * c + 0] = clamp_u8(img[(y * width + x + 1) * c + 0] + (err.x * 7 + 8) / 16);
            img[(y * width + x + 1) * c + 1] = clamp_u8(img[(y * width + x + 1) * c + 1] + (err.y * 7 + 8) / 16);
            img[(y * width + x + 1) * c + 2] = clamp_u8(img[(y * width + x + 1) * c + 2] + (err.z * 7 + 8) / 16);

            img[((y + 1) * width + x - 1) * c + 0] = clamp_u8(img[((y + 1) * width + x - 1) * c + 0] + (err.x * 3 + 8) / 16);
            img[((y + 1) * width + x - 1) * c + 1] = clamp_u8(img[((y + 1) * width + x - 1) * c + 1] + (err.y * 3 + 8) / 16);
            img[((y + 1) * width + x - 1) * c + 2] = clamp_u8(img[((y + 1) * width + x - 1) * c + 2] + (err.z * 3 + 8) / 16);

            img[((y + 1) * width + x) * c + 0] = clamp_u8(img[((y + 1) * width + x) * c + 0] + (err.x * 5 + 8) / 16);
            img[((y + 1) * width + x) * c + 1] = clamp_u8(img[((y + 1) * width + x) * c + 1] + (err.y * 5 + 8) / 16);
            img[((y + 1) * width + x) * c + 2] = clamp_u8(img[((y + 1) * width + x) * c + 2] + (err.z * 5 + 8) / 16);

            img[((y + 1) * width + x + 1) * c + 0] = clamp_u8(img[((y + 1) * width + x + 1) * c + 0] + (err.x * 1 + 8) / 16);
            img[((y + 1) * width + x + 1) * c + 1] = clamp_u8(img[((y + 1) * width + x + 1) * c + 1] + (err.y * 1 + 8) / 16);
            img[((y + 1) * width + x + 1) * c + 2] = clamp_u8(img[((y + 1) * width + x + 1) * c + 2] + (err.z * 1 + 8) / 16);
        }
    }
}


void unpad_img(uint8_t *img_rgb, int height, int width, int channels, const uint8_t *img_padded, int width_padded) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            memcpy(&img_rgb[(y * width + x) * channels],
                   &img_padded[((y + 1) * width_padded + (x + 1)) * channels],
                   channels);
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 3) die("program <input> <output>");

    const char *input = argv[1];
    const char *output = argv[2];

    int width, height, channels;
    uint8_t *img_rgb = stbi_load(input, &width, &height, &channels, 3);
    channels = 3;
    if (!img_rgb) die("Please fix the file");

    int width_padded = width + 2, height_padded = height + 2;
    uint8_t *img_padded = pad_image(img_rgb, width, height, channels);

    dither(width_padded, height_padded, img_padded);

    unpad_img(img_rgb, height, width, channels, img_padded, width_padded);

    // Can I use stride to reuse the padded img?
    int stride = width * channels;
    stbi_write_png(output, width, height, channels, img_rgb, stride);

    stbi_image_free(img_rgb);
    free(img_padded);
    return 0;
}
