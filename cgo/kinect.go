package main

/*
#cgo LDFLAGS: -L/usr/local/lib -lfreenect_sync -lfreenect
#include <stdio.h>
#include <stdlib.h>
#include <libfreenect/libfreenect_sync.h>
#include <libfreenect/libfreenect.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

func main() {
	// let's call it
	var data unsafe.Pointer
	var f_ctx *C.freenect_context
	var timestamp C.uint
	//var timestamp unsafe.Pointer
	C.freenect_init(&f_ctx, data)
	var nr_devices = C.freenect_num_devices(f_ctx)
	fmt.Println(nr_devices)
	val := C.freenect_sync_get_video(&data, &timestamp, 0, 0)
	fmt.Println(val)
	fmt.Println(timestamp)
}
