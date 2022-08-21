/**
 * \file CommandLineBarcodeScannerImageProcessingSample.c
 *
 * \brief ScanditSDK demo application
 *
 * Takes a list of input images and directories (containing images) as argument.
 * The resulting input images are processed in reverse order by the barcode
 * scanner.
 *
 * This example is configured to achieve a good scan performance on a single image
 * (not a video stream). We assume that we have infinite processing power and no
 * real time requirements.
 *
 * \copyright Copyright (c) 2018 Scandit AG. All rights reserved.
 */

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_surface.h>

#include <Scandit/ScRecognitionContext.h>
#include <Scandit/ScBarcodeScanner.h>

// Please insert your app key here:
#define SCANDIT_SDK_LICENSE_KEY "-- INSERT YOUR LICENSE KEY HERE --"

static char const * const ENABLED_FILE_EXTENSIONS[] = {
    "png",
    "jpg",
    "jpeg",
    "tif",
    "bmp",
    NULL
};

typedef struct InputImage {
    char const *file_name;
    struct InputImage const *next;
} InputImage;

static int has_valid_extension(char const *file_name)
{
    char const * const *extension = ENABLED_FILE_EXTENSIONS;
    for (; *extension != NULL; ++extension) {
        const size_t file_name_length = strlen(file_name);
        const size_t extension_length = strlen(*extension);

        char const * const file_extension =
                file_name + (file_name_length - extension_length);
        if (file_name_length >= extension_length &&
            strcmp(file_extension, *extension) == 0) {
            return SC_TRUE;
        }
    }

    return SC_FALSE;
}

static InputImage *push_if_valid_image(char const * filename,
                                      InputImage *container)
{
    if (has_valid_extension(filename) == SC_TRUE) {
        InputImage *new_image = malloc(sizeof(InputImage));
        const size_t target_size = strlen(filename) + 1;
        new_image->file_name = malloc(target_size);
        strncpy((char*)new_image->file_name, filename, target_size);

        new_image->next = container;

        container = new_image;
    }

    return container;
}

InputImage const *get_input_files(int argc, char const *argv[])
{
    InputImage *ret = NULL;

    // We skip the first argument
    for (int arg_idx = 1; arg_idx < argc; ++arg_idx) {
        char const * const current_arg = argv[arg_idx];

        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir(current_arg)) != NULL) {
            // We have a directory
            while ((ent = readdir (dir)) != NULL) {
                char *combined_name = malloc(sizeof(current_arg) +
                                             sizeof(ent->d_name) + 2);
                combined_name[0] = '\0';
                strcat(combined_name, current_arg);
                strcat(combined_name, "/");
                strcat(combined_name, ent->d_name);

                ret = push_if_valid_image(combined_name, ret);
                free(combined_name);
            }
            closedir (dir);
        } else {
            // We have a file
            ret = push_if_valid_image(current_arg, ret);
        }
    }

    return ret;
}

/**
 * Helper function to load image from disk using SDL2
 */
static ScBool load_image(const char* image_name, uint8_t** data,
                         uint32_t* width, uint32_t *height,
                         uint32_t* row_stride)
{
    SDL_Surface *image = IMG_Load(image_name);
    if (image == NULL) {
        printf("IMG_Load '%s' failed: %s\n", image_name, IMG_GetError());
        return SC_FALSE;
    }

    SDL_Surface* image_rgb = SDL_ConvertSurfaceFormat(image,
                                                      SDL_PIXELFORMAT_RGB24,
                                                      0);
    SDL_FreeSurface(image);
    if (image_rgb == NULL) {
        printf("Image '%s' convertion failed: %s\n", image_name, IMG_GetError());
        return SC_FALSE;
    }

    *width = image_rgb->w;
    *height = image_rgb->h;
    *row_stride = image_rgb->pitch;
    const int blob_size = *row_stride * *height;

    printf("Image '%s' size: %ux%u, stride %u (%u bytes)\n", image_name, *width, *height,
           *row_stride, blob_size);
    *data = malloc(blob_size);
    memcpy(*data, image_rgb->pixels, blob_size);

    SDL_FreeSurface(image_rgb);

    return SC_TRUE;
}

int main(int argc, const char *argv[])
{
    if (argc < 2) {
        printf("Please provide paths to image files or directories as arguments.\n");
        return -1;
    }
    printf("Scandit SDK Version: %s\n", SC_VERSION_STRING);

    int return_code = 0;

    ScRecognitionContext *context = NULL;
    ScBarcodeScanner *scanner = NULL;
    ScImageDescription *image_descr = NULL;
    ScBarcodeScannerSettings *settings = NULL;
    uint8_t *image_data = NULL;

    InputImage const * const images = get_input_files(argc, argv);

    // Create a recognition context. Files created by the recognition context and the
    // attached scanners will be written to this directory.  In production environment,
    // it should be replaced with writable path which does not get removed between reboots
    context = sc_recognition_context_new(SCANDIT_SDK_LICENSE_KEY, "/tmp", NULL);
    if (context == NULL) {
        printf("Could not initialize context.\n");
        return_code = -1;
        goto cleanup;
    }


    image_descr = sc_image_description_new();
    if (image_descr == NULL) {
        printf("Could not initialize image description.\n");
        return_code = -1;
        goto cleanup;
    }

    // The barcode scanner is configured by setting the appropriate properties on an 
    // "barcode scanner settings" instance. This settings object is passed to the barcode 
    // scanner when it is constructed. We start with the settings preset for single frame processing
    // and enable only the symbologies we need. For the purpose of this demo, we would like to scan
    // EAN13/UPCA and QR codes
    settings = sc_barcode_scanner_settings_new_with_preset(SC_PRESET_ENABLE_SINGLE_FRAME_MODE);
    if (settings == NULL) {
        printf("Could not initialize settings.\n");
        return_code = -1;
        goto cleanup;
    }
    sc_barcode_scanner_settings_set_symbology_enabled(settings, SC_SYMBOLOGY_EAN13, SC_TRUE);
    sc_barcode_scanner_settings_set_symbology_enabled(settings, SC_SYMBOLOGY_UPCA, SC_TRUE);
    sc_barcode_scanner_settings_set_symbology_enabled(settings, SC_SYMBOLOGY_QR, SC_TRUE);
    sc_barcode_scanner_settings_set_symbology_enabled(settings, SC_SYMBOLOGY_CODE128, SC_TRUE);

    // Set the symbol count for CODE 128 symbology to be in range from 4 to 20.
    ScSymbologySettings* ss = sc_barcode_scanner_settings_get_symbology_settings(settings, SC_SYMBOLOGY_CODE128);
    const uint16_t range_from = 4;
    const uint16_t range_to = 20;

    uint16_t sym_count[16];
    for(int i = range_from; i < range_to; ++i) {
        sym_count[i - range_from] = i;
    }
    sc_symbology_settings_set_active_symbol_counts(ss, sym_count, 16);

    // Set the maximum number of codes to look for in an image, 1 in our case.
    sc_barcode_scanner_settings_set_max_number_of_codes_per_frame(settings, 1);

    // By setting the code location constraints to ignore, we tell
    // the barcode scanner to search for codes in the whole image in every frame.
    sc_barcode_scanner_settings_set_code_location_constraint_1d(settings, SC_CODE_LOCATION_IGNORE);
    sc_barcode_scanner_settings_set_code_location_constraint_2d(settings, SC_CODE_LOCATION_IGNORE);

    // We make no assumptions about the most likely orientation of the codes.
    sc_barcode_scanner_settings_set_code_direction_hint(settings, SC_CODE_DIRECTION_NONE);

    // The barcode scanner allows to prevent codes from getting scanned again in
    // a certain time interval (e.g., 500ms). The default setting is 0 what
    // effectively disables this duplicate filtering.
    //sc_barcode_scanner_settings_set_code_duplicate_filter(settings, 500);

    // Create a barcode scanner for our context and settings.
    scanner = sc_barcode_scanner_new_with_settings(context, settings);
    if (scanner == NULL) {
        printf("Could not initialize scanner.\n");
        return_code = -1;
        goto cleanup;
    }

    // Wait for the initialization of the barcode scanner. We could omit this call
    // and start scanning immediately, but there is no guarantee that the barcode scanner 
    // operates at full capacity.
    if (!sc_barcode_scanner_wait_for_setup_completed(scanner)) {
        printf("barcode scanner setup failed.\n");
        return_code = -1;
        goto cleanup;
    }

    for (InputImage const *current_image = images; current_image != NULL;
            current_image = current_image->next) {
        // Load the image from disc.
        uint32_t image_width, image_height, row_stride;
        if (load_image(current_image->file_name, &image_data, &image_width,
                       &image_height, &row_stride) == SC_FALSE) {
            printf("Failed to load image '%s'.\n", current_image->file_name);
            return_code = -1;
            goto cleanup;
        }

        // Fill the image description for our loaded image.
        const uint32_t image_memory_size = row_stride * image_height;
        sc_image_description_set_layout(image_descr, SC_IMAGE_LAYOUT_RGB_8U);
        sc_image_description_set_width(image_descr, image_width);
        sc_image_description_set_height(image_descr, image_height);
        sc_image_description_set_first_plane_row_bytes(image_descr, row_stride);
        sc_image_description_set_memory_size(image_descr, image_memory_size);

        // Signal to the context that a new sequence of frames starts. This call is mandatory,
        // even if we are only going to process one image. Scanning will fail with
        // SC_RECOGNITION_CONTEXT_STATUS_FRAME_SEQUENCE_NOT_STARTED otherwise.
        sc_recognition_context_start_new_frame_sequence(context);

        ScProcessFrameResult result =
                sc_recognition_context_process_frame(context, image_descr,
                                                     image_data);
        if (result.status != SC_RECOGNITION_CONTEXT_STATUS_SUCCESS) {
            printf("Processing frame failed with error %d: '%s'\n", result.status,
                    sc_context_status_flag_get_message(result.status));
            return_code = -1;
            goto cleanup;
        }

        // Signal to the context that the frame sequence is finished.
        sc_recognition_context_end_frame_sequence(context);

        // Retrieve the barcode scanner object to get the list of codes that were recognized in
        // the last frame.
        ScBarcodeScannerSession *session = sc_barcode_scanner_get_session(scanner);

        // Get the list of codes that have been found in the last process frame call.
        ScBarcodeArray * new_codes =
                sc_barcode_scanner_session_get_newly_recognized_codes(session);
        uint32_t num_codes = sc_barcode_array_get_size(new_codes);
        if (num_codes == 0) {
            printf("no 1d or 2d barcodes found\n");
        }

        for (uint32_t i = 0; i < num_codes; ++i) {
            const ScBarcode * barcode = sc_barcode_array_get_item_at(new_codes, i);
            ScByteArray data = sc_barcode_get_data(barcode);
            ScSymbology symbology = sc_barcode_get_symbology(barcode);
            const char *symbology_name = sc_symbology_to_string(symbology);
            // For simplicity it is assumed that the barcode contains textual data, even
            // though it is possible to encode binary data in QR codes that contain null-
            // bytes at any position. For applications expecting binary data, use
            // sc_byte_array_get_data_size() to determine the length of the returned data.
            printf("barcode: symbology=%s, data='%s'\n", symbology_name, data.str);
        }
        sc_barcode_array_release(new_codes);

        free(image_data);
        image_data = NULL;
    }

cleanup:
    // Cleanup allocated data and objects. These functions all check for null values,
    // so it's save to pass in null objects.
    sc_barcode_scanner_release(scanner);
    sc_barcode_scanner_settings_release(settings);
    sc_recognition_context_release(context);
    sc_image_description_release(image_descr);

    free(image_data);
    for (InputImage const *current_image = images; current_image != NULL;) {
        InputImage const *next_image = current_image->next;

        free((char *)current_image->file_name);
        free((char *)current_image);

        current_image = next_image;
    }

    return return_code;
}
