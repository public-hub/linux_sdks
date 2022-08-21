all:
	gcc -O2 -std=c99 CommandLineBarcodeScannerImageProcessingSample.c -lscanditsdk -lz -lpthread -lSDL2 -lSDL2_image -o CommandLineBarcodeScannerImageProcessingSample
	gcc -O2 -std=c99 CommandLineBarcodeScannerCameraSample.c -lscanditsdk -lz -lpthread -o CommandLineBarcodeScannerCameraSample
	gcc -O2 -std=c99 CommandLineMatrixScanCameraSample.c -lscanditsdk -lz -lpthread -o CommandLineMatrixScanCameraSample
	gcc -O2 -std=c99 CommandLineBarcodeGeneratorSample.c -lscanditsdk -lz -lpthread -lpng -o CommandLineBarcodeGeneratorSample

clean:
	rm -f CommandLineBarcodeScannerImageProcessingSample
	rm -f CommandLineBarcodeScannerCameraSample
	rm -f CommandLineMatrixScanCameraSample
	rm -f CommandLineBarcodeGeneratorSample
