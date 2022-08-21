#!/usr/bin/env python

import scanditsdk as sc
import sys
import tempfile


def generate_settings(low_end_configuration=0):
    # Create barcode scanner with EAN13/UPCA and QR code scanning enabled.
    # The default preset is optimized for real-time frame processing using a camera.
    settings = sc.BarcodeScannerSettings()
    settings.enable_symbology(sc.SYMBOLOGY_EAN13, True)
    settings.enable_symbology(sc.SYMBOLOGY_UPCA, True)
    settings.enable_symbology(sc.SYMBOLOGY_QR, True)
    settings.code_location_area_1d = (0, 0.4), (1.0, 0.2)
    settings.focus_mode = sc.SC_CAMERA_FOCUS_MODE_FIXED

    # We want to scan at most one code per frame
    settings.max_number_of_codes_per_frame = 1

    if not low_end_configuration:
        # Search in the full image but occasionally check the code loaction too.
        settings.code_location_constraint_1d = sc.SC_CODE_LOCATION_HINT
        settings.code_location_constraint_2d = sc.SC_CODE_LOCATION_HINT
    else:
        # Scan the code location area exclusively.
        # This disables full image search to speed up processing
        settings.code_location_constraint_1d = sc.SC_CODE_LOCATION_RESTRICT
        settings.code_location_constraint_2d = sc.SC_CODE_LOCATION_RESTRICT

    # Only keep codes for one frame and do not accumulate anything.
    settings.code_duplicate_filter = 0
    settings.code_caching_duration = 0

    # Codes are most likely oriented from left to right.
    settings.code_direction_hint = sc.SC_CODE_DIRECTION_LEFT_TO_RIGHT

    return settings


def setup_camera():
    if len(sys.argv) == 2:
        # Setup the camera from a device path. E.g. /dev/video1
        camera = sc.Camera(str(sys.argv[1]))
    else:
        # When no parameters are given, the camera is automatically detected
        camera = sc.Camera()

    discrete_resolutions = camera.supported_resolutions.discrete_resolutions
    stepwise_resolutions = camera.supported_resolutions.stepwise_resolutions

    if discrete_resolutions:
        # The camera supports a small set of predefined resolutions
        print("Available discrete resolutions:")
        print(discrete_resolutions)
        camera.resolution = max(discrete_resolutions)

    if stepwise_resolutions:
        # The camera supports a wide range of resolutions that are generated step-wise.
        # Refer to documentation for further  explanation.
        print("Available stepwise resolutions:")
        print(stepwise_resolutions)
        camera.resolution = (
            stepwise_resolutions.max_width,
            stepwise_resolutions.max_height,
        )

    return camera


def run():
    #  Create a recognition context. Files created by the recognition context and the
    #  attached scanner will be written to a temporary directory. In production environment,
    #  it should be replaced with writable path which does not get removed between reboots.
    writable_temporary_directory = tempfile.gettempdir()
    context = sc.RecognitionContext(
        "-- INSERT YOUR LICENSE KEY HERE --", writable_temporary_directory
    )

    scanner_settings = generate_settings()
    camera = setup_camera()
    camera.start_streaming()
    scanner = sc.BarcodeScanner(context, scanner_settings)
    scanner.wait_for_setup_completed()

    frame_seq = context.start_new_frame_sequence()

    no_error = True
    while no_error:
        frame = camera.frame
        status = frame_seq.process_frame(frame.description, frame.data)

        if status.status != sc.RECOGNITION_CONTEXT_STATUS_SUCCESS:
            print(
                "Processing frame failed with code {}: {}".format(
                    status.status, status.get_status_flag_message()
                )
            )
            no_error = False

        codes = scanner.session.newly_recognized_codes
        if len(codes):
            for code in codes:
                print(
                    "Barcode found: ",
                    code.data,
                    " (",
                    code.symbology_string,
                    ")",
                    sep="",
                )

    frame_seq.end()


if __name__ == "__main__":
    run()
