#include <libfreenect/libfreenect_sync.h>
#include <libfreenect/libfreenect.h>
#include <microhttpd.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8888
#define BOUNDARY "--BORDER"

static const char *CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" BOUNDARY;

typedef struct {
  unsigned char *buffer;
  size_t size;
  size_t capacity;
} MemoryBuffer;

struct connection_state {
  enum { SEND_HEADER,
         SEND_DATA,
         FINISHED } state;
  size_t offset;
  freenect_frame_mode frame_info;
  MemoryBuffer* mem;
  unsigned int output_bit_depth;
  unsigned int PNG_COLOR_TYPE;
  int is_depth;
};

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

int image_to_PNG(MemoryBuffer * mem, unsigned char *data, freenect_frame_mode frame_info, int color_depth_output, int PNG_TYPE) {
  mem->capacity = frame_info.bytes;  // Initial capacity (estimate)
  mem->buffer = malloc(mem->capacity);
  if (!mem->buffer) {
    return -1;  // Memory allocation failed
  }

  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    free(mem->buffer);
    return -1;
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, (png_infopp)NULL);
    free(mem->buffer);
    return -1;
  }

  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    free(mem->buffer);
    return -1;
  }

  png_set_write_fn(png, mem, write_data, NULL);


  png_set_IHDR(png, info, frame_info.width, frame_info.height, color_depth_output,  PNG_TYPE,
                PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  for (int16_t y = 0; y < frame_info.height; y++) {
    png_bytep row = data + (y * frame_info.width * (frame_info.data_bits_per_pixel+frame_info.padding_bits_per_pixel)/8);  // 3 bytes per pixel
    png_write_row(png, row);
  }

  // Write the image data

  png_write_end(png, NULL);

  png_destroy_write_struct(&png, &info);
  return 0;
}

static ssize_t content_reader(void *cls, uint64_t pos, char *buf, size_t max) {
  struct connection_state *conn = (struct connection_state *)cls;

  if (conn->state == FINISHED) return 0;

  if (conn->state == SEND_HEADER) {
    printf("SENDING FRAME HEADER\n");
    const char *header =
        "\r\n--" BOUNDARY
        "\r\n"
        "Content-Type: image/jpeg\r\n\r\n";

    char header_buf[256];
    int header_len = snprintf(header_buf, sizeof(header_buf), header);
    printf(header_buf);
    size_t to_copy = header_len - conn->offset;
    if (to_copy > max) to_copy = max;

    memcpy(buf, header_buf + conn->offset, to_copy);
    conn->offset += to_copy;

    if (conn->offset >= header_len) {
      conn->state = SEND_DATA;
      conn->offset = 0;
      printf("CREATING PNG\n");
      // Create a PNG image from the RGB data
      unsigned char *data;
      unsigned int timestamp;
      if (conn->is_depth==0){
        freenect_sync_get_video_with_res((void **)(&data), &timestamp, 0, conn->frame_info.resolution, conn->frame_info.video_format);
      }
      else{
        freenect_sync_get_depth_with_res((void **)(&data), &timestamp, 0, conn->frame_info.resolution, conn->frame_info.depth_format);
      }
      conn->mem->size = 0;
      if (image_to_PNG(conn->mem, data, conn->frame_info, conn->output_bit_depth, conn->PNG_COLOR_TYPE)) {
        printf("ERROR IN PNG");
        return MHD_NO;
      }
    }
    return to_copy;
  }

  if (conn->state == SEND_DATA) {
    size_t remaining = conn->mem->size - conn->offset;
    printf("REMAINING DATA: %d\n", remaining);
    if (remaining == 0) {
      printf("SENT DATA\n");
      conn->state = SEND_HEADER;
      conn->offset = 0;
      free(conn->mem->buffer);
      return 0;
    }

    size_t to_copy = (remaining > max) ? max : remaining;
    memcpy(buf, conn->mem->buffer + conn->offset, to_copy);
    conn->offset += to_copy;
    return to_copy;
  }

  return 0;
}

static void free_connection_state(void *cls) {
  free(cls);
}

enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                     const char *url,
                                     const char *method, const char *version,
                                     const char *upload_data,
                                     long unsigned int *upload_data_size, void **con_cls) {
  // Assume you have the raw RGB data in a buffer
  if (strcmp("/rgb", url) && strcmp("/depth", url) && strcmp("/feed_rgb", url) && strcmp("/feed_depth", url) && strcmp("/feed_ir", url)) {
    return MHD_NO;
  }

  if (strncmp("/feed_", url, 6) == 0) {
    if (*con_cls == NULL) {
      // First call - initialize connection state
      struct connection_state *state = malloc(sizeof(*state));
      state->state = SEND_HEADER;
      state->offset = 0;
      *con_cls = state;
      state->mem = malloc(sizeof(MemoryBuffer));
      state->mem->buffer = NULL;
      state->mem->capacity = 0;
      state->mem->size = 0;
      if (strcmp("/feed_rgb", url) == 0) {
        state->frame_info = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
        state->is_depth=0;
        state->output_bit_depth = 8;
        state->PNG_COLOR_TYPE = PNG_COLOR_TYPE_RGB;
      }
      if (strcmp("/feed_ir", url) == 0) {
        state->frame_info = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_8BIT);
        state->is_depth = 0;
        state->output_bit_depth = 8;
        state->PNG_COLOR_TYPE = PNG_COLOR_TYPE_GRAY;
      }
      if (strcmp("/feed_depth", url) == 0) {
        state->frame_info = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_10BIT);
        state->is_depth=1;
        state->output_bit_depth = 16;
        state->PNG_COLOR_TYPE = PNG_COLOR_TYPE_GRAY;
      }

      return MHD_YES;
    }
    printf("STARTING RESPONSE\n");
    // Create streaming response
    struct MHD_Response *response = MHD_create_response_from_callback(
        MHD_SIZE_UNKNOWN, 4096,
        &content_reader,
        *con_cls,
        &free_connection_state);
    // char* bytes_str;
    // sprintf(bytes_str, "%s", frame_info.bytes);
    MHD_add_response_header(response, "Content-Type", CONTENT_TYPE);
    // MHD_add_response_header(response, "Cache-Control", "no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0");
    // MHD_add_response_header(response, "Pragma", "no-cache");
    // MHD_add_response_header(response, "Connection", "close");
    // MHD_add_response_header(response, "Content-Length", "1921600");

    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
  }
  unsigned char *data;
  int output_bit_depth;
  int PNG_COLOR_TYPE;
  unsigned int timestamp;

  freenect_frame_mode frame_info;
  if (strcmp("/rgb", url) == 0) {
      frame_info = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
      freenect_sync_get_video_with_res((void **)(&data), &timestamp, 0, frame_info.resolution, frame_info.video_format);
      output_bit_depth = 8;
      PNG_COLOR_TYPE = PNG_COLOR_TYPE_RGB;
    }
  if (strcmp("/depth", url) == 0) {
    frame_info = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_10BIT);
    freenect_sync_get_depth_with_res((void**)&data, &timestamp, 0, frame_info.resolution, frame_info.depth_format);
    output_bit_depth = 16;
    PNG_COLOR_TYPE = PNG_COLOR_TYPE_GRAY;
  }
  printf("Resolution: %dx%d\nBits per Pixel:%d+%d\n", frame_info.width, frame_info.height, frame_info.data_bits_per_pixel, frame_info.padding_bits_per_pixel);
  printf("Total bytes: %d %d\n", frame_info.bytes, (frame_info.width) * (frame_info.height) * (frame_info.data_bits_per_pixel + frame_info.padding_bits_per_pixel) / 8);
  // Create a PNG image from the RGB data
  MemoryBuffer mem = {NULL, 0, 0};
  if (image_to_PNG(&mem, data, frame_info, output_bit_depth, PNG_COLOR_TYPE)) {
    printf("ERROR IN PNG");
    return MHD_NO;
  }

  struct MHD_Response *response;
  int ret;

  response = MHD_create_response_from_buffer(mem.size,
                                             (void *)mem.buffer, MHD_RESPMEM_MUST_FREE);
  MHD_add_response_header(response, "Content-Type", "image/png");
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
