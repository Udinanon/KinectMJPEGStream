#include <jpeglib.h>
#include <stdio.h>

int main(void) {
    struct jpeg_compress_struct cinfo;
    jpeg_create_compress(&cinfo);
    printf("jpeg_CreateCompress succeeded\n");
    jpeg_destroy_compress(&cinfo);
    return 0;
}
