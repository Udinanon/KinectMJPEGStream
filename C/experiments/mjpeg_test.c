#include <libfreenect/libfreenect.h>
#include <libfreenect/libfreenect_sync.h>
#include <microhttpd.h>
#include <stdint.h>
#include <string.h>
#include <png.h>
#include <stdlib.h>

#define BOUNDARY "--BORDER"
static const char *CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" BOUNDARY;

struct connection_state {
  enum { SEND_HEADER,
    SEND_DATA,
    FINISHED } state;
    size_t offset;
  };
  
typedef struct {
  unsigned char *buffer;
  size_t size;
  size_t capacity;
} MemoryBuffer;
  
MemoryBuffer mem = {NULL, 0, 0};

void write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
  MemoryBuffer *mem = (MemoryBuffer *)png_get_io_ptr(png_ptr);

  // Ensure there's enough space in the buffer
  if (mem->size + length > mem->capacity) {
    mem->capacity = (mem->size + length) * 2;  // Increase capacity
    mem->buffer = realloc(mem->buffer, mem->capacity);
    if (!mem->buffer) {
      png_error(png_ptr, "Memory allocation failed\n");
    }
  }

  // Copy the data to the buffer
  memcpy(mem->buffer + mem->size, data, length);
  mem->size += length;
}

freenect_frame_mode frame_info;

static ssize_t content_reader(void *cls, uint64_t pos, char *buf, size_t max) {
  struct connection_state *conn = (struct connection_state *)cls;
  (void)pos;  // We ignore position, manage state ourselves

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
      freenect_sync_get_video_with_res((void **)(&data), &timestamp, 0, frame_info.resolution, frame_info.video_format);
      mem.capacity = frame_info.bytes;  // Initial capacity (estimate)
      mem.buffer = malloc(mem.capacity);
      mem.size = 0;
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

      png_set_IHDR(png, info, frame_info.width, frame_info.height, 8, PNG_COLOR_TYPE_RGB,
                   PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                   PNG_FILTER_TYPE_DEFAULT);
      png_write_info(png, info);
      for (int16_t y = 0; y < frame_info.height; y++) {
        png_bytep row = data + (y * frame_info.width * 3);  // 3 bytes per pixel
        png_write_row(png, row);
      }

      // Write the image data

      png_write_end(png, NULL);

      png_destroy_write_struct(&png, &info);
    }
    return to_copy;
  }

  if (conn->state == SEND_DATA) {
    size_t remaining = mem.size - conn->offset;
    printf("REMAINING DATA: %d\n", remaining);
    if (remaining == 0) {
      printf("SENT DATA\n");
      conn->state = SEND_HEADER;
      conn->offset = 0;
      return 0;
    }

    size_t to_copy = (remaining > max) ? max : remaining;
    memcpy(buf, mem.buffer + conn->offset, to_copy);
    conn->offset += to_copy;
    return to_copy;
  }

  return 0;
}

static void free_connection_state(void *cls) {
  free(cls);
}

static enum MHD_Result request_handler(void *cls,
                                       struct MHD_Connection *connection,
                                       const char *url,
                                       const char *method,
                                       const char *version,
                                       const char *upload_data,
                                       size_t *upload_data_size,
                                       void **con_cls) {
  (void)cls;
  (void)version;
  (void)upload_data;
  (void)upload_data_size;

  if (*con_cls == NULL) {
    // First call - initialize connection state
    struct connection_state *state = malloc(sizeof(*state));
    state->state = SEND_HEADER;
    state->offset = 0;
    *con_cls = state;
    return MHD_YES;
  }
  printf("STARTING RESPONSE\n");
  // Create streaming response
  struct MHD_Response *response = MHD_create_response_from_callback(
      MHD_SIZE_UNKNOWN, 4096,
      &content_reader,
      *con_cls,
      &free_connection_state);
  //char* bytes_str;
  //sprintf(bytes_str, "%s", frame_info.bytes);
  MHD_add_response_header(response, "Content-Type", CONTENT_TYPE);
  //MHD_add_response_header(response, "Cache-Control", "no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0");
  //MHD_add_response_header(response, "Pragma", "no-cache");
  //MHD_add_response_header(response, "Connection", "close");
  //MHD_add_response_header(response, "Content-Length", "1921600");

  enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

// Main setup (minimal version)
int main() {
  frame_info = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);

  struct MHD_Daemon *daemon = MHD_start_daemon(
      MHD_USE_THREAD_PER_CONNECTION,
      8080, NULL, NULL,
      &request_handler, NULL,
      MHD_OPTION_END);

  if (!daemon) return 1;
  getchar();  // Wait for keypress
  MHD_stop_daemon(daemon);
  return 0;
}