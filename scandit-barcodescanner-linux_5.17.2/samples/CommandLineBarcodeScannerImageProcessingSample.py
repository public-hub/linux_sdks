#!/usr/bin/env python
# This example configures the SDK for a single image use case without any resource restrictions.

from __future__ import print_function
import sys
import tempfile
import sdl2.ext
import scanditsdk as sc

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Please provide a path to an image file as argument.")
        exit(1)

    image_path = sys.argv[1]
    image_surface = sdl2.ext.load_image(image_path, enforce="SDL")

    # We convert the image to RGB for consistency.
    # Gray or any YUV formats are recommended if speed is critical.
    rgb_image_surface = sdl2.SDL_ConvertSurfaceFormat(
        image_surface, sdl2.SDL_PIXELFORMAT_RGB24, 0
    )

    descr = sc.ImageDescription()
    descr.width = rgb_image_surface.contents.w
    descr.height = rgb_image_surface.contents.h
    descr.first_plane_row_bytes = rgb_image_surface.contents.pitch
    descr.layout = sc.IMAGE_LAYOUT_RGB_8U
    descr.memory_size = rgb_image_surface.contents.pitch * rgb_image_surface.contents.h

    # Create a recognition context. Files created by the recognition context and the
    # attached scanner will be written to a temporary directory. In production environment,
    # it should be replaced with writable path which does not get removed between reboots.
    writable_temporary_directory = tempfile.gettempdir()
    context = sc.RecognitionContext(
        "-- INSERT YOUR LICENSE KEY HERE --", writable_temporary_directory
    )

    # Use the single frame preset.
    settings = sc.BarcodeScannerSettings(preset=sc.PRESET_ENABLE_SINGLE_FRAME_MODE)

    # We assume the worst camera system.
    settings.focus_mode = sc.SC_CAMERA_FOCUS_MODE_FIXED

    # We want to scan at most one code per frame.
    # Should be set to a higher number if false positives are likely.
    settings.max_number_of_codes_per_frame = 1

    # Search in the full image and do not check the code location area.
    settings.code_location_constraint_1d = sc.SC_CODE_LOCATION_IGNORE
    settings.code_location_constraint_2d = sc.SC_CODE_LOCATION_IGNORE

    # No assumptions about the code direction are made.
    settings.code_direction_hint = sc.SC_CODE_DIRECTION_NONE

    settings.enable_symbology(sc.SYMBOLOGY_EAN13, True)
    settings.enable_symbology(sc.SYMBOLOGY_UPCA, True)
    settings.enable_symbology(sc.SYMBOLOGY_QR, True)
    settings.enable_symbology(sc.SYMBOLOGY_CODE128, True)

    # Change the EAN13 symbology settings to enable scanning of color-inverted barcodes.
    symbology_settings = settings.symbologies[sc.SYMBOLOGY_EAN13]
    symbology_settings.color_inverted_enabled = True

    scanner = sc.BarcodeScanner(context, settings)
    scanner.wait_for_setup_completed()

    frame_seq = context.start_new_frame_sequence()
    status = frame_seq.process_frame(descr, rgb_image_surface.contents.pixels)
    frame_seq.end()

    if status.status != sc.RECOGNITION_CONTEXT_STATUS_SUCCESS:
        print(
            "Processing frame failed with code {}: {}".format(
                status.status, status.get_status_flag_message()
            )
        )
        exit(2)

    codes = scanner.session.newly_recognized_codes
    if len(codes):
        for code in codes:
            print(
                "Barcode found at location ",
                code.location,
                ": ",
                code.data,
                " (",
                code.symbology_string,
                ")",
                sep="",
            )
            # We can either iterate over all corner points (clockwise starting at the top left corner)
            # of the code location or access them individually.
            for corner in code.location.all_corners:
                print(corner)
            print("Top right corner: ", code.location.top_right, sep="")
    else:
        print("No barcode found.")
