#!/usr/bin/env python

import scanditsdk as sc
from PIL import Image
import tempfile

if __name__ == "__main__":
    #  Create a recognition context. Files created by the recognition context and the
    #  attached scanner will be written to a temporary directory. In production environment,
    #  it should be replaced with writable path which does not get removed between reboots.
    writable_temporary_directory = tempfile.gettempdir()
    context = sc.RecognitionContext(
        "-- INSERT YOUR LICENSE KEY HERE --", writable_temporary_directory
    )

    symbology = sc.SYMBOLOGY_QR
    generator = sc.BarcodeGenerator(context, symbology)
    barcode_data = "Hello World! | 1234567890"
    options = {
        "foregroundColor": [0, 0, 0, 255],
        "backgroundColor": [255, 255, 255, 255],
        "errorCorrectionLevel": "H",
    }

    generator.set_options(options)
    result = generator.generate(barcode_data)

    channels = 4  # the generated barcode is RGBA
    pixels = result.numpy_array.reshape(
        result.description.width, result.description.height, channels
    )

    image = Image.fromarray(pixels.astype("uint8"), "RGBA")
    image.save("output.png")
