#include <libfreenect/libfreenect_sync.h>
#include <microhttpd.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8888

typedef struct {
  unsigned char *buffer;
  size_t size;
  size_t capacity;
} MemoryBuffer;

void write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
  MemoryBuffer *mem = (MemoryBuffer *)png_get_io_ptr(png_ptr);

  // Ensure there's enough space in the buffer
  if (mem->size + length > mem->capacity) {
    mem->capacity = (mem->size + length) * 2;  // Increase capacity
    mem->buffer = realloc(mem->buffer, mem->capacity);
    if (!mem->buffer) {
      png_error(png_ptr, "Memory allocation failed");
    }
  }

  // Copy the data to the buffer
  memcpy(mem->buffer + mem->size, data, length);
  mem->size += length;
}

enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                     const char *url,
                                     const char *method, const char *version,
                                     const char *upload_data,
                                     long unsigned int *upload_data_size, void **con_cls) {
  // Assume you have the raw RGB data in a buffer
  unsigned char *data;
  unsigned int timestamp;
  freenect_sync_get_video((void **)(&data), &timestamp, 0, FREENECT_VIDEO_RGB);
  int width = 640/* width of the image */;
  int height = 480/* height of the image */;

  // Create a PNG image from the RGB data
  MemoryBuffer mem = {NULL, 0, 0};
  mem.capacity = width * height * 4;  // Initial capacity (estimate)
  mem.buffer = malloc(mem.capacity);
  if (!mem.buffer) {
    return 1;  // Memory allocation failed
  }

  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    free(mem.buffer);
    return 1;
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, (png_infopp)NULL);
    free(mem.buffer);
    return 1;
  }

  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    free(mem.buffer);
    return 1;
  }

  png_set_write_fn(png, &mem, write_data, NULL);
  png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);

  // Write the image data
  for (int y = 0; y < height; y++) {
    png_bytep row = data + (y * width * 3);  // 3 bytes per pixel
    png_write_row(png, row);
  }
  png_write_end(png, NULL);

  png_destroy_write_struct(&png, &info);

  char* page;
  asprintf(&page, "<html><body>Hello, browser! Timestamp= %u </body></html>", timestamp);

  struct MHD_Response *response;
  int ret;
  //response = MHD_create_response_from_buffer(strlen(page),
  //                                           (void *)page, MHD_RESPMEM_PERSISTENT);
  //MHD_add_response_header(response, "Content-Type", "image/png");
  response = MHD_create_response_from_buffer(mem.size,
                                             (void *)mem.buffer, MHD_RESPMEM_PERSISTENT);
  MHD_add_response_header(response, "Content-Type", "image/png");
  //MHD_add_response_header(response, "Content-Length", (const char *)str);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

int main() {

  struct MHD_Daemon *daemon;


  daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, PORT, NULL, NULL,
                            &answer_to_connection, NULL, MHD_OPTION_END);
  if (NULL == daemon) return 1;
  getchar();

  MHD_stop_daemon(daemon);
  return 0;
}
