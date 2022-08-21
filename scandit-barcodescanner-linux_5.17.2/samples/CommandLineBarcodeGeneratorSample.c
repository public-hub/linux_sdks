/**
 * \file CommandLineBarcodeGeneratorSample.c
 *
 * \brief ScanditSDK demo application
 *
 * \copyright Copyright (c) 2018 Scandit AG. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <png.h>

#include <Scandit/ScRecognitionContext.h>
#include <Scandit/ScBarcodeGenerator.h>

// Please insert your app key here:
#define SCANDIT_SDK_LICENSE_KEY "-- INSERT YOUR LICENSE KEY HERE --"

#define BARCODE_DATA "Hello World! | 1234567890"
#define OUTPUT_FILE "output.png"

int main(int argc, const char *argv[])
{
    const uint8_t* data = (uint8_t*) BARCODE_DATA;
    size_t data_length = strlen(BARCODE_DATA);

    printf("Scandit SDK Version: %s\n", SC_VERSION_STRING);

    ScRecognitionContext *context = NULL;
    ScBarcodeGenerator *generator = NULL;
    ScImageBuffer *image = NULL;
    ScError error;

    // Create a recognition context. Files created by the recognition context and the
    // attached scanners will be written to this directory.  In production environment,
    // it should be replaced with writable path which does not get removed between reboots
    context = sc_recognition_context_new(SCANDIT_SDK_LICENSE_KEY, "/tmp", NULL);
    if (context == NULL) {
        printf("Could not initialize context.\n");
        return 1;
    }

    // Set the desired symbology and options.
    ScSymbology symbology = SC_SYMBOLOGY_QR;
    const char * OPTIONS =
        "{"
        "   \"foregroundColor\" : [0, 0, 0, 255],"
        "   \"backgroundColor\" : [255, 255, 255, 255],"
        "   \"errorCorrectionLevel\" : \"H\""
        "}";
    // The code is assumed to be ASCII from start to end.
    ScEncodingArray encoding = sc_encoding_array_new(1);
    sc_encoding_array_assign(&encoding, 0, "US-ASCII", 0, data_length);

    // Create the barcode generator object.
    generator = sc_barcode_generator_new_with_options(context, symbology, OPTIONS, &error);
    if (generator == NULL) {
        printf("Could create generator object: %s\n", error.message);
        return 1;
    }

    // Generate the barcode.
    image = sc_barcode_generator_generate(generator, data, data_length, encoding, &error);
    if (image == NULL) {
        printf("Could not generate image: %s\n", error.message);
        return 1;
    }
    size_t width = sc_image_description_get_width(image->description);
    size_t height = sc_image_description_get_height(image->description);


    // Write image to file using libPNG
    FILE *fp = fopen(OUTPUT_FILE, "wb");
    if (fp == NULL) {
        printf("Could not open file %s.\n", OUTPUT_FILE);
        return 1;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
        printf("Failed to create png write struct.\n");
        return 1;
    }
    png_infop info = png_create_info_struct(png);
    if (info == NULL) {
        printf("Failed to create png info struct.\n");
        return 1;
    }
    png_init_io(png, fp);
    png_set_IHDR(png,
                  info,
                  width, height,
                  8,
                  PNG_COLOR_TYPE_RGBA,
                  PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT
                 );
    png_byte** rows = (png_byte**)malloc(height * sizeof(png_byte*));
    for (size_t i = 0; i < height; i++) {
        rows[i] = (png_byte*) &(image->data[width*4*i]);
    }
    png_set_rows(png, info, rows);
    png_write_png(png, info, 0, NULL);
    free(rows);
    png_destroy_write_struct(&png, &info);
    fclose(fp);

    // Clean up.
    sc_image_buffer_free(image);
    sc_barcode_generator_free(generator);
    sc_encoding_array_free(encoding);
    sc_recognition_context_release(context);
}
