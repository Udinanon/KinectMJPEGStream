package main

/*
#cgo LDFLAGS: -L/usr/local/lib -lfreenect_sync -lfreenect
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libfreenect/libfreenect_sync.h>
#include <libfreenect/libfreenect.h>
*/
import "C"
import (
	"fmt"
	"image"
	"image/color"
	"image/png"
	"os"
	"unsafe"
)

func main() {
	// Initial test, number of conected Kinects
	var init_data unsafe.Pointer
	var f_ctx *C.freenect_context
	C.freenect_init(&f_ctx, init_data)
	var nr_devices = C.freenect_num_devices(f_ctx)
	fmt.Println("Active Kinects detected: ", nr_devices)

	//Collect info about output resolution
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)
	fmt.Println("Resolution:", frame_info.width, "x", frame_info.height, "\nBits per Pixel:", frame_info.data_bits_per_pixel, "+", frame_info.padding_bits_per_pixel)
	fmt.Println("Total bytes:", frame_info.bytes, (C.int)(frame_info.width)*(C.int)(frame_info.height)*(C.int)(frame_info.data_bits_per_pixel+frame_info.padding_bits_per_pixel)/8)

	// Get the video frrame
	var data unsafe.Pointer
	var timestamp C.uint
	val := C.freenect_sync_get_video(&data, &timestamp, 0, C.FREENECT_VIDEO_RGB)
	fmt.Println(val)
	//fmt.Println(timestamp)

	// Optional: copy data out, as it's allocated by C
	var memory_segment = C.malloc(C.size_t(frame_info.bytes))
	fmt.Println((*C.char)(data))
	fmt.Println((*C.char)(memory_segment))
	C.memcpy(memory_segment, data, C.size_t(frame_info.bytes))

	// Parse data into a Go bytes array and save as PNG
	var image_data = C.GoBytes(data, frame_info.bytes)
	RGB_image := image.NewRGBA(image.Rect(0, 0, int(frame_info.width), int(frame_info.height)))
	for y := 0; y < int(frame_info.height); y++ {
		for x := 0; x < int(frame_info.width); x++ {
			// Calculate the index in the RGB data
			index := (y*int(frame_info.width) + x) * 3
			r := image_data[index]
			g := image_data[index+1]
			b := image_data[index+2]

			// Set the pixel in the image
			RGB_image.Set(x, y, color.RGBA{r, g, b, 255}) // 255 for full opacity
		}
	}
	// Save the image to a file
	file, err := os.Create("output.png")
	if err != nil {
		panic(err)
	}
	defer file.Close()

	// Encode the image to PNG format
	if err := png.Encode(file, RGB_image); err != nil {
		panic(err)
	}
}
