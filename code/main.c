#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include "lib/ppm.h"
#include "lib/cycletimer.h"

#define LTRTWALLDENOM 7
#define TPWALLDENOM 8
#define BUCKET_SIZE 32
#define COLORS 256
#define BCTHRESH_DECIMAL 0.005
#define FILTER_SIZE 50

typedef struct {
    int xmin, xmax, ymin, ymax;
} range;

static int getBucketIdx(int r, int g, int b) {
    int buckets = COLORS / BUCKET_SIZE;
    return r * buckets * buckets + g * buckets + b;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: %s <infile> <outfile>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *infile = argv[1];
    char *outfile = argv[2];

    uint64_t ticks;

    printf("begin\n");
    ticks = currentTicks();

    // Initializtion
    PPMImage *img = readPPM(infile);
    if (img == NULL) {
        exit(EXIT_FAILURE);
    }

    printf("load image: %" PRIu64 "\n", currentTicks() - ticks);
    ticks = currentTicks();

    // Get Walls
    int ltWall = img->width / LTRTWALLDENOM;
    int rtWall = (img->width * (LTRTWALLDENOM - 1)) / LTRTWALLDENOM;
    int tpWall = img->height / TPWALLDENOM;

    // Get color distribution
    int buckets = COLORS / BUCKET_SIZE;
    int *color_counts = malloc(buckets * buckets * buckets * sizeof(int));
    if (color_counts == NULL) {
        exit(EXIT_FAILURE);
    }

    range rs[3];
    rs[0].xmin = 0;
    rs[0].xmax = ltWall;
    rs[0].ymin = 0;
    rs[0].ymax = img->height;
    rs[1].xmin = rtWall;
    rs[1].xmax = img->width;
    rs[1].ymin = 0;
    rs[1].ymax = img->height;
    rs[2].xmin = 0;
    rs[2].xmax = img->width;
    rs[2].ymin = 0;
    rs[2].ymax = tpWall;

    int i, j, ri;
    for (ri = 0; ri < 3; ri++) {
        range r = rs[ri];
        for (i = r.ymin; i < r.ymax; i++) {
            for (j = r.xmin; j < r.xmax; j++) {
                PPMPixel *pt = getPixel(j, i, img);
                color_counts[
                    getBucketIdx(
                        pt->red / BUCKET_SIZE,
                        pt->green / BUCKET_SIZE,
                        pt->blue / BUCKET_SIZE)
                ] += 1;
            }
        }
    }

    printf("get color_counts: %" PRIu64 "\n", currentTicks() - ticks);
    ticks = currentTicks();

    int totalBCPix =
        ltWall * img->height +
        (img->width - rtWall) * img->height +
        tpWall * img->width;

    int bcThresh = BCTHRESH_DECIMAL * totalBCPix;

    char *oldMask = calloc(img->width * img->height, sizeof(char));
    if (oldMask == NULL) {
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < img->height; i++) {
        for (j = 0; j < img->width; j++) {
            PPMPixel *pt = getPixel(j, i, img);
            unsigned char r = pt->red / BUCKET_SIZE;
            unsigned char g = pt->green / BUCKET_SIZE;
            unsigned char b = pt->blue / BUCKET_SIZE;
            if (color_counts[getBucketIdx(r, g, b)] < bcThresh) {
                oldMask[i * img->width + j] = 1;
            }
        }
    }

    printf("get oldMask: %" PRIu64 "\n", currentTicks() - ticks);
    ticks = currentTicks();

    char *mask = calloc(img->width * img->height, sizeof(char));
    if (mask == NULL) {
        exit(EXIT_FAILURE);
    }
    memcpy(mask, oldMask, img->width * img->height * sizeof(char));

    // Clean up mask
    for (i = 2; i < img->height - 2; i++) {
        for (j = 2; j < img->width - 2; j++) {
            char this = oldMask[i * img->width + j];
            if (this == 0) {
                int borderSum =
                    oldMask[(i - 1) * img->width + j] +
                    oldMask[i * img->width + j - 1] +
                    oldMask[(i + 1) * img->width + j] +
                    oldMask[i * img->width + j + 1] +
                    oldMask[(i - 2) * img->width + j] +
                    oldMask[i * img->width + j - 2] +
                    oldMask[(i + 2) * img->width + j] +
                    oldMask[i * img->width + j + 2];
                if (borderSum >= 2) {
                    mask[i * img->width + j] = 1;
                }
            }
        }
    }

    printf("get mask: %" PRIu64 "\n", currentTicks() - ticks);
    ticks = currentTicks();

    // Blur
    printf("finished mask, starting blur\n");
    float *blurKernel = malloc(FILTER_SIZE * FILTER_SIZE * sizeof(float));
    if (blurKernel == NULL) {
        exit(EXIT_FAILURE);
    }
    // Even box blur
    for (i = 0; i < FILTER_SIZE; i++) {
        for (j = 0; j < FILTER_SIZE; j++) {
            blurKernel[i * FILTER_SIZE + j] = 1.0;
        }
    }

    PPMPixel *blurData = calloc(img->width * img->height, sizeof(PPMPixel));
    if (blurData == NULL) {
        exit(EXIT_FAILURE);
    }

    int width = img->width;
    int height = img->height;
    int row, col;
    for (row = 0; row < height; row++) {
        if (row % 100 == 0) {
            printf("finished row %d\n", row);
        }
        for (col = 0; col < width; col++) {
            float count = 0;
            int i_k, j_k;
            float red = 0;
            float green = 0;
            float blue = 0;
            for (i_k = 0; i_k < FILTER_SIZE; i_k++) {
                for (j_k = 0; j_k < FILTER_SIZE; j_k++) {
                    float weight = blurKernel[i_k * FILTER_SIZE + j_k];
                    int i = row - (FILTER_SIZE / 2) + i_k;
                    int j = col - (FILTER_SIZE / 2) + j_k;

                    if (i < 0 || i >= height || j < 0 || j >= width) {
                        continue;
                    } else if (mask[i * width + j] == 1) {
                        continue;
                    }
                    PPMPixel *pt = getPixel(j, i, img);
                    red += weight * (pt->red);
                    green += weight * (pt->green);
                    blue += weight * (pt->blue);
                    count += weight;
                }
            }
            if (count == 0) {
                continue;
            }
            if (row == 33 && col == 0) {
                printf("red %f\n", red / count);
            }
            blurData[row * width + col].red = (unsigned char)(red / count);
            blurData[row * width + col].green = (unsigned char)(green / count);
            blurData[row * width + col].blue = (unsigned char)(blue / count);
        }
    }

    printf("get blurData: %" PRIu64 "\n", currentTicks() - ticks);
    ticks = currentTicks();

    // Put filter on mask
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            if (mask[i * width + j] == 1) {
                PPMPixel *pt = getPixel(j, i, img);
                blurData[i * width + j].red = pt->red;
                blurData[i * width + j].green = pt->green;
                blurData[i * width + j].blue = pt->blue;
            }
        }
    }


    PPMPixel *oldData = img->data;
    img->data = blurData;

    printf("use blurData: %" PRIu64 "\n", currentTicks() - ticks);
    ticks = currentTicks();

    errno = 0;
    writePPM(outfile, img);
    if (errno != 0) {
        exit(EXIT_FAILURE);
    }

    printf("write image: %" PRIu64 "\n", currentTicks() - ticks);

    free(oldData);
    free(color_counts);
    free(blurKernel);
    free(img);
    free(img->data);
    return 0;
}
