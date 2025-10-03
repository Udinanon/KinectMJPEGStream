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
	"bytes"
	"errors"
	"fmt"
	"image"
	"image/color"
	"image/draw"
	"image/png"
	"io"
	"mime/multipart"
	"net/http"
	"net/textproto"
	"os"
	"unsafe"
)

func getRoot(w http.ResponseWriter, r *http.Request) {
	fmt.Printf("got / request\n")
	io.WriteString(w, "This is my website!\n")
}

func get_image(w http.ResponseWriter, r *http.Request, frame_info C.freenect_frame_mode, frame_handler func(frame_info C.freenect_frame_mode) image.Image) {
	fmt.Printf("got image request\n")
	w.Header().Set("Content-Type", "image/png")
	var image = frame_handler(frame_info)
	buf := new(bytes.Buffer)
	err := png.Encode(buf, image)
	if err != nil {
		panic(err)
	}
	w.Write(buf.Bytes())
}

// thanks https://github.com/nsmith5/mjpeg/blob/main/mjpeg.go
func get_MJPEG_feed(w http.ResponseWriter, r *http.Request, frame_info C.freenect_frame_mode, frame_handler func(frame_info C.freenect_frame_mode) image.Image) {
	fmt.Println("Resolution:", frame_info.width, "x", frame_info.height, "\nBits per Pixel:", frame_info.data_bits_per_pixel, "+", frame_info.padding_bits_per_pixel)
	fmt.Println("Total bytes:", frame_info.bytes, (C.int)(frame_info.width)*(C.int)(frame_info.height)*(C.int)(frame_info.data_bits_per_pixel+frame_info.padding_bits_per_pixel)/8)

	mimeWriter := multipart.NewWriter(w)
	mimeWriter.SetBoundary("--boundary")
	contentType := fmt.Sprintf("multipart/x-mixed-replace;boundary=%s", mimeWriter.Boundary())
	w.Header().Add("Cache-Control", "no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0")
	w.Header().Add("Content-Type", contentType)
	w.Header().Add("Pragma", "no-cache")
	w.Header().Add("Connection", "close")

	for {
		partHeader := make(textproto.MIMEHeader)
		partHeader.Add("Content-Type", "image/jpeg")
		var img = frame_handler(frame_info)

		partWriter, partErr := mimeWriter.CreatePart(partHeader)
		if nil != partErr {
			fmt.Println(fmt.Sprintf(partErr.Error()))
			break
		}
		buf := new(bytes.Buffer)

		err := png.Encode(buf, img)
		if err != nil {
			return
		}
		if _, writeErr := partWriter.Write(buf.Bytes()); nil != writeErr {
			fmt.Println(fmt.Sprintf(writeErr.Error()))
		}

	}
}

func main() {

	http.HandleFunc("/", getRoot)

	http.HandleFunc("/omni_feed", func(w http.ResponseWriter, r *http.Request) {
		var frame_info C.freenect_frame_mode
		//frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)
		get_MJPEG_feed(w, r, frame_info, get_omni_image)
	})

	http.HandleFunc("/rgb", func(w http.ResponseWriter, r *http.Request) {
		var frame_info C.freenect_frame_mode
		frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)
		get_image(w, r, frame_info, get_RGB_image)
	})

	http.HandleFunc("/depth", func(w http.ResponseWriter, r *http.Request) {
		var frame_info C.freenect_frame_mode
		frame_info = C.freenect_find_depth_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_DEPTH_10BIT)
		get_image(w, r, frame_info, get_depth_image)
	})

	http.HandleFunc("/ir", func(w http.ResponseWriter, r *http.Request) {
		var frame_info C.freenect_frame_mode
		frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_IR_8BIT)
		get_image(w, r, frame_info, get_ir_image)
	})

	http.HandleFunc("/bayer", func(w http.ResponseWriter, r *http.Request) {
		var frame_info C.freenect_frame_mode
		frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_BAYER)
		get_image(w, r, frame_info, get_bayer_image)
	})

	http.HandleFunc("/rgb_feed", func(w http.ResponseWriter, r *http.Request) {
		var frame_info C.freenect_frame_mode
		frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)
		get_MJPEG_feed(w, r, frame_info, get_RGB_image)
	})

	http.HandleFunc("/depth_feed", func(w http.ResponseWriter, r *http.Request) {
		var frame_info C.freenect_frame_mode
		frame_info = C.freenect_find_depth_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_DEPTH_10BIT)
		get_MJPEG_feed(w, r, frame_info, get_depth_image)
	})

	http.HandleFunc("/ir_feed", func(w http.ResponseWriter, r *http.Request) {
		var frame_info C.freenect_frame_mode
		frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_IR_8BIT)
		get_MJPEG_feed(w, r, frame_info, get_ir_image)
	})

	http.HandleFunc("/bayer_feed", func(w http.ResponseWriter, r *http.Request) {
		var frame_info C.freenect_frame_mode
		frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_BAYER)
		get_MJPEG_feed(w, r, frame_info, get_bayer_image)
	})

	fmt.Println("0.0.0.0:3333/rgb_feed")
	fmt.Println("0.0.0.0:3333/ir_feed")
	fmt.Println("0.0.0.0:3333/depth_feed")
	fmt.Println("0.0.0.0:3333/bayer_feed")

	err := http.ListenAndServe(":3333", nil)
	if errors.Is(err, http.ErrServerClosed) {
		fmt.Printf("server closed\n")
	} else if err != nil {
		fmt.Printf("error starting server: %s\n", err)
		os.Exit(1)

	}
}

func get_omni_image(frame_info C.freenect_frame_mode) image.Image {
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_IR_8BIT)
	ir_image := get_ir_image(frame_info)

	frame_info = C.freenect_find_depth_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_DEPTH_10BIT)
	depth_image := get_depth_image(frame_info)

	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)
	//rgb_image := get_RGB_image(frame_info)
	//rgb_image.Bounds().Dx() +
	compound_width := depth_image.Bounds().Dx() + ir_image.Bounds().Dx()
	max_hieght := max(depth_image.Bounds().Dy(), ir_image.Bounds().Dy()) //, rgb_image.Bounds().Dy())
	larger_rect := image.Rect(0, 0, compound_width, max_hieght)
	omni_image := image.NewRGBA(larger_rect)
	draw.Draw(omni_image, ir_image.Bounds(), ir_image, image.Point{0, 0}, draw.Src)
	draw.Draw(omni_image, depth_image.Bounds().Add(image.Point{ir_image.Bounds().Dx(), 0}), depth_image, image.Point{0, 0}, draw.Src)
	//draw.Draw(omni_image, ir_image.Bounds(), ir_image, image.Point{ir_image.Bounds().Max.X + depth_image.Bounds().Max.X, 0}, draw.Src)

	return omni_image
}

func get_ir_image(frame_info C.freenect_frame_mode) image.Image {

	// Get the video frrame
	var data unsafe.Pointer
	var timestamp C.uint
	//val :=
	C.freenect_sync_get_video_with_res(&data, &timestamp, 0, C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_IR_8BIT)

	// Parse data into a Go bytes array and then as image.Image
	var image_data = C.GoBytes(data, frame_info.bytes)
	RGB_image := image.NewRGBA(image.Rect(0, 0, int(frame_info.width), int(frame_info.height)))
	for y := 0; y < int(frame_info.height); y++ {
		for x := 0; x < int(frame_info.width); x++ {
			// Calculate the index in the RGB data
			index := (y*int(frame_info.width) + x)
			r := image_data[index]
			g := image_data[index]
			b := image_data[index]

			// Set the pixel in the image
			RGB_image.Set(x, y, color.RGBA{r, g, b, 255}) // 255 for full opacity
		}
	}
	return RGB_image
}

func get_bayer_image(frame_info C.freenect_frame_mode) image.Image {

	// Get the video frrame
	var data unsafe.Pointer
	var timestamp C.uint

	C.freenect_sync_get_video_with_res(&data, &timestamp, 0, C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_BAYER)

	// Parse data into a Go bytes array and then as image.Image
	var image_data = C.GoBytes(data, frame_info.bytes)
	RGB_image := image.NewRGBA(image.Rect(0, 0, int(frame_info.width), int(frame_info.height)))
	for y := 0; y < int(frame_info.height); y++ {
		for x := 0; x < int(frame_info.width); x++ {
			// Calculate the index in the RGB data
			index := (y*int(frame_info.width) + x)
			r := image_data[index]
			g := image_data[index]
			b := image_data[index]

			// Set the pixel in the image
			RGB_image.Set(x, y, color.RGBA{r, g, b, 255}) // 255 for full opacity
		}
	}
	return RGB_image
}

func get_depth_image(frame_info C.freenect_frame_mode) image.Image {

	// Get the video frrame
	var data unsafe.Pointer
	var timestamp C.uint
	//val :=
	C.freenect_sync_get_depth_with_res(&data, &timestamp, 0, C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_DEPTH_10BIT)

	RGB_image := image.NewRGBA(image.Rect(0, 0, int(frame_info.width), int(frame_info.height)))
	for y := 0; y < int(frame_info.height); y++ {
		for x := 0; x < int(frame_info.width); x++ {
			// Calculate the index in the RGB data

			index := (y*int(frame_info.width) + x)
			col_16 := (*(*C.uint16_t)(unsafe.Pointer(uintptr(data) + uintptr(index)*unsafe.Sizeof(uint16(0)))))
			r := uint8(col_16 >> 2)
			b := uint8(col_16 << 4)
			// Set the pixel in the image
			RGB_image.SetRGBA(x, y, color.RGBA{r, 0, b, 255})
		}
	}
	return RGB_image
}

func get_RGB_image(frame_info C.freenect_frame_mode) image.Image {

	// Get the video frrame
	var data unsafe.Pointer
	var timestamp C.uint
	C.freenect_sync_get_video_with_res(&data, &timestamp, 0, C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)

	// Parse data into a Go bytes array and then as image.Image
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

	return RGB_image
}
