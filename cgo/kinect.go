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
func getRGB(w http.ResponseWriter, r *http.Request) {
	fmt.Printf("got /rgb request\n")
	w.Header().Set("Content-Type", "image/png")
	var image = get_RGB_image(false)
	buf := new(bytes.Buffer)
	err := png.Encode(buf, image)
	if err != nil {
		panic(err)
	}
	w.Write(buf.Bytes())
}

// thanks https://github.com/nsmith5/mjpeg/blob/main/mjpeg.go
func getRGB_feed(w http.ResponseWriter, r *http.Request) {
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)
	fmt.Println("Resolution:", frame_info.width, "x", frame_info.height, "\nBits per Pixel:", frame_info.data_bits_per_pixel, "+", frame_info.padding_bits_per_pixel)
	fmt.Println("Total bytes:", frame_info.bytes, (C.int)(frame_info.width)*(C.int)(frame_info.height)*(C.int)(frame_info.data_bits_per_pixel+frame_info.padding_bits_per_pixel)/8)

	mimeWriter := multipart.NewWriter(w)
	mimeWriter.SetBoundary("--boundary")
	contentType := fmt.Sprintf("multipart/x-mixed-replace;boundary=%s", mimeWriter.Boundary())
	w.Header().Add("Cache-Control", "no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0")
	w.Header().Add("Content-Type", contentType)
	w.Header().Add("Pragma", "no-cache")
	w.Header().Add("Connection", "close")
	w.Header().Add("Content-Length", string(int(frame_info.bytes)))

	//boundary := "\r\n--frame\r\nContent-Type: image/png\r\n\r\n"
	for {
		partHeader := make(textproto.MIMEHeader)
		partHeader.Add("Content-Type", "image/jpeg")
		var img = get_RGB_image(false)

		//n, err := io.WriteString(w, boundary)
		//if err != nil || n != len(boundary) {
		//	return
		//}
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
		// n, err = io.WriteString(w, "\r\n")
		// if err != nil || n != 2 {
		// 	return
		// }
	}
}

func getDepth_feed(w http.ResponseWriter, r *http.Request) {
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_DEPTH_10BIT)
	fmt.Println("Resolution:", frame_info.width, "x", frame_info.height, "\nBits per Pixel:", frame_info.data_bits_per_pixel, "+", frame_info.padding_bits_per_pixel)
	fmt.Println("Total bytes:", frame_info.bytes, (C.int)(frame_info.width)*(C.int)(frame_info.height)*(C.int)(frame_info.data_bits_per_pixel+frame_info.padding_bits_per_pixel)/8)

	mimeWriter := multipart.NewWriter(w)
	mimeWriter.SetBoundary("--boundary")
	contentType := fmt.Sprintf("multipart/x-mixed-replace;boundary=%s", mimeWriter.Boundary())
	w.Header().Add("Cache-Control", "no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0")
	w.Header().Add("Content-Type", contentType)
	w.Header().Add("Pragma", "no-cache")
	w.Header().Add("Connection", "close")
	w.Header().Add("Content-Length", string(int(frame_info.bytes)))

	//boundary := "\r\n--frame\r\nContent-Type: image/png\r\n\r\n"
	for {
		partHeader := make(textproto.MIMEHeader)
		partHeader.Add("Content-Type", "image/jpeg")
		var img = get_depth_image()

		//n, err := io.WriteString(w, boundary)
		//if err != nil || n != len(boundary) {
		//	return
		//}
		partWriter, partErr := mimeWriter.CreatePart(partHeader)
		if nil != partErr {
			fmt.Println(fmt.Sprintf(partErr.Error()))
			break
		}
		buf := new(bytes.Buffer)

		//err := jpeg.Encode(buf, img, &jpeg.Options{Quality: 75})
		err := png.Encode(buf, img)
		if err != nil {
			return
		}
		if _, writeErr := partWriter.Write(buf.Bytes()); nil != writeErr {
			fmt.Println(fmt.Sprintf(writeErr.Error()))
		}
		// n, err = io.WriteString(w, "\r\n")
		// if err != nil || n != 2 {
		// 	return
		// }
	}
}
func getBayer_feed(w http.ResponseWriter, r *http.Request) {
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_BAYER)

	mimeWriter := multipart.NewWriter(w)
	mimeWriter.SetBoundary("--boundary")
	contentType := fmt.Sprintf("multipart/x-mixed-replace;boundary=%s", mimeWriter.Boundary())
	w.Header().Add("Cache-Control", "no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0")
	w.Header().Add("Content-Type", contentType)
	w.Header().Add("Pragma", "no-cache")
	w.Header().Add("Connection", "close")
	w.Header().Add("Content-Length", string(int(frame_info.bytes)))

	//boundary := "\r\n--frame\r\nContent-Type: image/png\r\n\r\n"
	for {
		partHeader := make(textproto.MIMEHeader)
		partHeader.Add("Content-Type", "image/jpeg")
		var img = get_bayer_image()

		//n, err := io.WriteString(w, boundary)
		//if err != nil || n != len(boundary) {
		//	return
		//}
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

func getIR_feed(w http.ResponseWriter, r *http.Request) {
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_IR_8BIT)
	fmt.Println("Resolution:", frame_info.width, "x", frame_info.height, "\nBits per Pixel:", frame_info.data_bits_per_pixel, "+", frame_info.padding_bits_per_pixel)
	fmt.Println("Total bytes:", frame_info.bytes, (C.int)(frame_info.width)*(C.int)(frame_info.height)*(C.int)(frame_info.data_bits_per_pixel+frame_info.padding_bits_per_pixel)/8)

	mimeWriter := multipart.NewWriter(w)
	mimeWriter.SetBoundary("--boundary")
	contentType := fmt.Sprintf("multipart/x-mixed-replace;boundary=%s", mimeWriter.Boundary())
	w.Header().Add("Cache-Control", "no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0")
	w.Header().Add("Content-Type", contentType)
	w.Header().Add("Pragma", "no-cache")
	w.Header().Add("Connection", "close")
	w.Header().Add("Content-Length", string(int(frame_info.bytes)))

	//boundary := "\r\n--frame\r\nContent-Type: image/png\r\n\r\n"
	for {
		partHeader := make(textproto.MIMEHeader)
		partHeader.Add("Content-Type", "image/jpeg")
		var img = get_ir_image()

		//n, err := io.WriteString(w, boundary)
		//if err != nil || n != len(boundary) {
		//	return
		//}
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

func getYUV_feed(w http.ResponseWriter, r *http.Request) {
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)
	fmt.Println("Resolution:", frame_info.width, "x", frame_info.height, "\nBits per Pixel:", frame_info.data_bits_per_pixel, "+", frame_info.padding_bits_per_pixel)
	fmt.Println("Total bytes:", frame_info.bytes, (C.int)(frame_info.width)*(C.int)(frame_info.height)*(C.int)(frame_info.data_bits_per_pixel+frame_info.padding_bits_per_pixel)/8)

	mimeWriter := multipart.NewWriter(w)
	mimeWriter.SetBoundary("--boundary")
	contentType := fmt.Sprintf("multipart/x-mixed-replace;boundary=%s", mimeWriter.Boundary())
	w.Header().Add("Cache-Control", "no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0")
	w.Header().Add("Content-Type", contentType)
	w.Header().Add("Pragma", "no-cache")
	w.Header().Add("Connection", "close")
	w.Header().Add("Content-Length", string(int(frame_info.bytes)))

	//boundary := "\r\n--frame\r\nContent-Type: image/png\r\n\r\n"
	for {
		partHeader := make(textproto.MIMEHeader)
		partHeader.Add("Content-Type", "image/jpeg")
		var img = get_YUV_image()

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
	http.HandleFunc("/rgb", getRGB)
	http.HandleFunc("/rgb_feed", getRGB_feed)
	http.HandleFunc("/depth_feed", getDepth_feed)
	http.HandleFunc("/ir_feed", getIR_feed)
	http.HandleFunc("/yuv_feed", getYUV_feed)
	http.HandleFunc("/bayer_feed", getBayer_feed)

	err := http.ListenAndServe(":3333", nil)
	if errors.Is(err, http.ErrServerClosed) {
		fmt.Printf("server closed\n")
	} else if err != nil {
		fmt.Printf("error starting server: %s\n", err)
		os.Exit(1)

	}
}

func get_ir_image() image.Image {

	//Collect info about output resolution
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_IR_8BIT)

	// Get the video frrame
	var data unsafe.Pointer
	var timestamp C.uint
	//val :=
	C.freenect_sync_get_video_with_res(&data, &timestamp, 0, C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_IR_8BIT)

	// Parse data into a Go bytes array and save as PNG
	//var image_data = C.GoBytes(data, frame_info.bytes)

	// Parse data into a Go bytes array and save as PNG
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

func get_bayer_image() image.Image {

	//Collect info about output resolution
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_BAYER)

	// Get the video frrame
	var data unsafe.Pointer
	var timestamp C.uint
	//val :=
	C.freenect_sync_get_video_with_res(&data, &timestamp, 0, C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_BAYER)

	// Parse data into a Go bytes array and save as PNG
	//var image_data = C.GoBytes(data, frame_info.bytes)

	// Parse data into a Go bytes array and save as PNG
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

func get_depth_image() image.Image {

	//Collect info about output resolution
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_DEPTH_10BIT)

	// Get the video frrame
	var data unsafe.Pointer
	var timestamp C.uint
	//val :=
	C.freenect_sync_get_depth_with_res(&data, &timestamp, 0, C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_DEPTH_10BIT)

	// Parse data into a Go bytes array and save as PNG
	//var image_data = C.GoBytes(data, frame_info.bytes)

	//var u16_data = (*C.ushort)(data)
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

func get_RGB_image(save bool) image.Image {

	//Collect info about output resolution
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)

	// Get the video frrame
	var data unsafe.Pointer
	var timestamp C.uint
	C.freenect_sync_get_video_with_res(&data, &timestamp, 0, C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)

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

	if save {
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
	return RGB_image
}

func get_YUV_image() image.Image {

	//Collect info about output resolution
	var frame_info C.freenect_frame_mode
	frame_info = C.freenect_find_video_mode(C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_RGB)

	// Get the video frrame
	var data unsafe.Pointer
	var timestamp C.uint
	//val :=
	C.freenect_sync_get_video_with_res(&data, &timestamp, 0, C.FREENECT_RESOLUTION_MEDIUM, C.FREENECT_VIDEO_YUV_RGB)

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

	return RGB_image
}
