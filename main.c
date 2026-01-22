#include <stdio.h>
#include <stdint.h>
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

uint8x3 quantize(uint8x3 c, uint8x3 *palette, uint32_t len) {
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

uint8_t *img_at(uint8_t *img, int x, int y, int width, int channels) {
    return &img[(y * width + x) * channels];
}

void dither(int width, int height, uint8_t *img) {
    // next cell error
    int32x3 error = {0};
    // next row errors, padded
    int32x3 *errors = malloc((2 + width) * sizeof(int32x3));
    memset(errors, 0, (2 + width) * sizeof(int32x3));

    int c = 3;
    for (int y = 0; y < height; y++) {
        // first we propagate the previous row errors
        for (int x = 0; x < width; x++) {
            uint8_t *pixel = img_at(img, x, y, width, c);
            pixel[0] = clamp_u8(pixel[0] + errors[x].x);
            pixel[1] = clamp_u8(pixel[1] + errors[x].y);
            pixel[2] = clamp_u8(pixel[2] + errors[x].z);
        }

        // then we reset the errors
        memset(errors, 0, (2 + width) * sizeof(int32x3));
        error = (int32x3){0};

        // then we propagate the previous cell error, quantize and calculate the new errors
        for (int x = 0; x < width; x++) {
            uint8x3 pixel = read_uint8x3(img_at(img, x, y, width, c));
            pixel.x = clamp_u8(pixel.x + error.x);
            pixel.y = clamp_u8(pixel.y + error.y);
            pixel.z = clamp_u8(pixel.z + error.z);

            uint8x3 new_pixel = quantize(pixel, palette, palette_length);
            img_at(img, x, y, width, c)[0] = new_pixel.x;
            img_at(img, x, y, width, c)[1] = new_pixel.y;
            img_at(img, x, y, width, c)[2] = new_pixel.z;

            int32x3 err = (int32x3){
                (int32_t)pixel.x - new_pixel.x,
                (int32_t)pixel.y - new_pixel.y,
                (int32_t)pixel.z - new_pixel.z,
            };

            // next cell     : 7/16
            error = (int32x3){
                err.x * 7 / 16,
                err.y * 7 / 16,
                err.z * 7 / 16,
            };
            // next row - 1  : 3/16
            errors[x] = (int32x3){
                errors[x].x + err.x * 3 / 16,
                errors[x].y + err.y * 3 / 16,
                errors[x].z + err.z * 3 / 16,
            };
            // next row      : 5/16
            errors[x+1] = (int32x3){
                errors[x+1].x + err.x * 5 / 16,
                errors[x+1].y + err.y * 5 / 16,
                errors[x+1].z + err.z * 5 / 16,
            };
            // next row + 1  : 1/16
            errors[x+2] = (int32x3){
                errors[x+2].x + err.x * 1 / 16,
                errors[x+2].y + err.y * 1 / 16,
                errors[x+2].z + err.z * 1 / 16,
            };
        }
    }
    free(errors);
}

int main(int argc, char **argv) {
    if (argc != 3) die("program <input> <output>");

    const char *input = argv[1];
    const char *output = argv[2];

    int width, height, channels;
    uint8_t *img = stbi_load(input, &width, &height, &channels, 3);
    channels = 3;
    if (!img) die("Please fix the file");

    dither(width, height, img);

    int stride = width * channels;
    stbi_write_png(output, width, height, channels, img, stride);

    stbi_image_free(img);
    return 0;
}
