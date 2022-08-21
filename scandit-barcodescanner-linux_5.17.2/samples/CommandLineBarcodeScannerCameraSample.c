/**
 * \file CommandLineBarcodeScannerCameraSample.c
 *
 * This Scandit SDK sample application demonstrates how to use a V4L2 camera
 * as a frame source for barcode scanning. This sample does not include
 * a user interface. Scanned codes will be shown on the command line.
 *
 * If you don't provide any command line options /dev/video0 with the default
 * resolution defined below will be used.
 *
 * To select a different device or resolution you can provide the device path
 * and the desired resolution width and height as command line arguments.
 *
 * Example:
 * ./CommandLineBarcodeScannerCameraSample /dev/video1 640 480
 *
 * \copyright Copyright (c) 2018 Scandit AG. All rights reserved.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <Scandit/ScRecognitionContext.h>
#include <Scandit/ScBarcodeScanner.h>
#include <Scandit/ScCamera.h>

// Please insert your app key here:
#define SCANDIT_SDK_LICENSE_KEY "-- INSERT YOUR LICENSE KEY HERE --"

// Please insert the desired default camera resolution here:
#define DEFAULT_RESOLUTION_WIDTH 1280
#define DEFAULT_RESOLUTION_HEIGHT 720

// Set this define to 1 of you run the demo on a device with slow hardware or no GPU support
// such as a Raspberry Pi Zero.
// It disables barcode search and only scans codes in the center image area.
#define LOW_END_DEVICE_CONFIGURATION 0

static volatile ScBool process_frames;

static void catch_exit(int signo) {
    printf("SIGINT received.\n");
    process_frames = SC_FALSE;
}

static void print_all_discrete_resolutions(const ScCamera *cam) {
    printf("This camera uses discrete resolutions:\n");
    ScSize resolution_array[20];
    ScFramerate framerate_array[10];
    const int32_t resolution_count = sc_camera_query_supported_resolutions(cam, &resolution_array[0], 20);
    for (int32_t i = 0; i < resolution_count; i++) {
        const int32_t framerate_count = sc_camera_query_supported_framerates(cam, resolution_array[i], framerate_array, 10);
        for (int32_t j = 0; j < framerate_count; j++) {
            const float fps = sc_framerate_get_fps(&framerate_array[j]);
            printf("\t%u:%u @ %.2f FPS\n", resolution_array[i].width, resolution_array[i].height, fps);
        }
    }
}

int main(int argc, const char *argv[]) {
    // Handle ctrl+c events.
    if (signal(SIGINT, catch_exit) == SIG_ERR) {
        printf("Could not set up signal handler.\n");
        return -1;
    }

    // Create the camera object.
    ScCamera *camera = NULL;
    if (argc > 1) {
        // Setup the camera from a device path. E.g. /dev/video1
        // We use 4 image buffers.
        camera = sc_camera_new_from_path(argv[1], 4);
    } else {
        // When no parameters are given, the camera is automatically detected.
        camera = sc_camera_new();
    }

    if (camera == NULL) {
        printf("No camera available.\n");
        return -1;
    }

    uint32_t resolution_width = DEFAULT_RESOLUTION_WIDTH;
    uint32_t resolution_height = DEFAULT_RESOLUTION_HEIGHT;
    // Read the desired resolution form the command line.
    if (argc == 4) {
        resolution_width = atoi(argv[2]);
        resolution_height = atoi(argv[3]);
    }

    // Get the supported resolutions and check
    // if the desired resolution is supported.
    ScCameraMode resm = sc_camera_get_resolution_mode(camera);
    ScBool supported = SC_FALSE;
    const uint32_t resolutions_size = 30;
    ScSize resolutions[resolutions_size];
    int32_t resolutions_found;
    ScStepwiseResolution swres;

    switch (resm) {
        case SC_CAMERA_MODE_DISCRETE:
            print_all_discrete_resolutions(camera);

            // The camera supports a small set of predefined resolutions
             resolutions_found = sc_camera_query_supported_resolutions(camera, &resolutions[0], resolutions_size);
            if (!resolutions_found) {
                printf("There was an error getting the discrete resolution capabilities of the camera.\n");
                return -1;
            }

            for (int i = 0; i < resolutions_found; i++) {
                if (resolutions[i].width == resolution_width &&
                    resolutions[i].height == resolution_height) {
                    supported = SC_TRUE;
                    break;
                }
            }
            break;

        case SC_CAMERA_MODE_STEPWISE:
            // The camera supports a wide range of resolutions that are
            // generated step-wise. Refer to documentation for further
            // explanation.
            if (!sc_camera_query_supported_resolutions_stepwise(camera, &swres)) {
                printf("There was an error getting the stepwise resolution capabilities of the camera.\n");
                return -1;
            }

            printf("This camera uses step-wise resolutions:\n");
            printf("\tx: %u:%u:%u\n", swres.min_width, swres.step_width, swres.max_width);
            printf("\ty: %u:%u:%u\n", swres.min_height, swres.step_height, swres.max_height);

            if (swres.min_width <= resolution_width &&
                resolution_width <= swres.max_width &&
                swres.min_height <= resolution_height &&
                resolution_height <= swres.max_height &&
                resolution_width % swres.step_width == 0 &&
                resolution_height % swres.step_height == 0) {
                supported = SC_TRUE;
            }
            break;

        default:
            printf("Could not get camera resolution mode.\n");
            return -1;
    }

    // Set the resolution
    if (!supported) {
        printf("%dx%d is not supported by this camera.\nPlease specify a supported resolution on the command line or in the source code.\n", resolution_width, resolution_height);
        sc_camera_release(camera);
        return -1;
    }

    ScSize desired_resolution;
    desired_resolution.width = resolution_width;
    desired_resolution.height =  resolution_height;
    if (!sc_camera_request_resolution(camera, desired_resolution)) {
        printf("Setting resolution failed.\n");
        sc_camera_release(camera);
        return -1;
    }

    // Start streaming.
    if (!sc_camera_start_stream(camera)) {
        printf("Start the camera failed.\n");
        sc_camera_release(camera);
        return -1;
    }

    // Create a recognition context. Files created by the recognition context and the
    // attached scanners will be written to this directory.  In production environment,
    // it should be replaced with writable path which does not get removed between reboots
    ScRecognitionContext *context =
            sc_recognition_context_new(SCANDIT_SDK_LICENSE_KEY, "/tmp", NULL);
    if (context == NULL) {
        printf("Could not initialize context.\n");
        sc_camera_release(camera);
        return -1;
    }

    // Create barcode scanner with EAN13/UPCA and QR code scanning enabled.
    // The default preset is optimized for real-time frame processing using a
    // camera.
    ScBarcodeScannerSettings *settings =
        sc_barcode_scanner_settings_new_with_preset(SC_PRESET_NONE);
    if (settings == NULL) {
        sc_recognition_context_release(context);
        sc_camera_release(camera);
        return -1;
    }
    sc_barcode_scanner_settings_set_symbology_enabled(settings, SC_SYMBOLOGY_EAN13, SC_TRUE);
    sc_barcode_scanner_settings_set_symbology_enabled(settings, SC_SYMBOLOGY_UPCA, SC_TRUE);
    sc_barcode_scanner_settings_set_symbology_enabled(settings, SC_SYMBOLOGY_QR, SC_TRUE);

    // We want to scan at most one code per frame.
    sc_barcode_scanner_settings_set_max_number_of_codes_per_frame(settings, 1);

    // We define the center area of the image to be the most likely location for a 1d barcode.
    ScRectangleF code_location = { .position.x = 0.f, .position.y = 0.4f,
                                   .size.width = 1.0f, .size.height = 0.2f };
    sc_barcode_scanner_settings_set_code_location_area_1d(settings, code_location);
    // We keep the area for 2d codes to the default (whole image).

#if LOW_END_DEVICE_CONFIGURATION
    // Scan at the code location area exclusively.
    // This disables full image search to speed up procesing.
    sc_barcode_scanner_settings_set_code_location_constraint_1d(settings, SC_CODE_LOCATION_RESTRICT);
    sc_barcode_scanner_settings_set_code_location_constraint_2d(settings, SC_CODE_LOCATION_RESTRICT);
#else
    // Search in the full image but occasionally check the code loaction too.
    sc_barcode_scanner_settings_set_code_location_constraint_1d(settings, SC_CODE_LOCATION_HINT);
    sc_barcode_scanner_settings_set_code_location_constraint_2d(settings, SC_CODE_LOCATION_HINT);
#endif

    // Only keep codes for one frame and do not accumulate anything.
    sc_barcode_scanner_settings_set_code_duplicate_filter(settings, 0);
    sc_barcode_scanner_settings_set_code_caching_duration(settings, 0);

    // Our camera has no auto-focus.
    sc_barcode_scanner_settings_set_focus_mode(settings, SC_CAMERA_FOCUS_MODE_FIXED);
    // Codes are most likely oriented from left to right.
    sc_barcode_scanner_settings_set_code_direction_hint(settings, SC_CODE_DIRECTION_LEFT_TO_RIGHT);

    // Create a barcode scanner for our context and settings.
    ScBarcodeScanner *scanner = sc_barcode_scanner_new_with_settings(context ,settings);
    sc_barcode_scanner_settings_release(settings);
    if (scanner == NULL) {
        sc_recognition_context_release(context);
        sc_camera_release(camera);
        return -1;
    }
    // The scanner is setup asynchronous.
    // We could wait here using sc_barcode_scanner_wait_for_setup_completed if needed.

    // Access the barcode scanner session. It collects all the results.
    ScBarcodeScannerSession *session = sc_barcode_scanner_get_session(scanner);

    // Signal a new frame sequence to the context.
    sc_recognition_context_start_new_frame_sequence(context);

    // Create an image description that is reused for every frame.
    ScImageDescription * image_descr = sc_image_description_new();
    process_frames = SC_TRUE;
    while (process_frames) {
        // Get the latest camera frame data and description.
        const uint8_t *image_data = sc_camera_get_frame(camera, image_descr);
        if (image_data == NULL) {
            printf("Frame access failed. Exiting.\n");
            break;
        }

        // Process the frame.
        ScProcessFrameResult result = sc_recognition_context_process_frame(context, image_descr, image_data);
        if (result.status != SC_RECOGNITION_CONTEXT_STATUS_SUCCESS) {
            printf("Processing frame failed with error %d: '%s'\n", result.status,
                   sc_context_status_flag_get_message(result.status));
        }

        // Get the results. If there is a barcode, print it!
        ScBarcodeArray * new_codes = sc_barcode_scanner_session_get_newly_recognized_codes(session);
        int code_count = sc_barcode_array_get_size(new_codes);
        for (int i = 0; i < code_count; i++) {
            const ScBarcode * code = sc_barcode_array_get_item_at(new_codes, i);
            ScByteArray data = sc_barcode_get_data(code);
            printf("Barcode found: '%s'\n", data.str);
        }

        // Signal the camera that we are done reading the image buffer.
        sc_camera_enqueue_frame_data(camera, image_data);

        // Cleanup the memory we used.
        sc_barcode_array_release(new_codes);
    }

    // Signal to the context that the frame sequence is finished.
    sc_recognition_context_end_frame_sequence(context);

    // Cleanup all objects.
    sc_image_description_release(image_descr);
    sc_barcode_scanner_release(scanner);
    sc_recognition_context_release(context);
    sc_camera_release(camera);
}
