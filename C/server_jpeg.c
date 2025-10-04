// Necessary for some fucntions and constants like FREENECT_RESOLUTION_MEDIUM and freenect_find_depth_mode()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libfreenect/libfreenect.h>
// For freenect_sync_get_video_with_res()
#include <jerror.h>
#include <jpeglib.h>
#include <libfreenect/libfreenect_sync.h>
#include <microhttpd.h>

// Local network port
#define PORT 8888

// border between MJPEG frames 
#define BOUNDARY "--BORDER"
// sutomatically interpolated value to a const string
static const char *CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" BOUNDARY;

// Buffer containing JPEG image data
typedef struct {
  unsigned char *buffer;
  size_t size;
} MemoryBuffer;

// information about both the internal state (HEADER, DATA, FINISHED) and details such as what kind of info was requested (RGB, Depth)
struct connection_state {
  enum { SEND_HEADER,
         SEND_DATA,
         FINISHED } state; // What stage of the connection we're in
  size_t offset; // when writing large images we need top send multiple blocks with an offset
  freenect_frame_mode frame_info; // identifies specifics on captiured image data such as size, bit depth, type
  MemoryBuffer* mem; // containing the JPEG data
  unsigned int is_monochrome; // for libjpeg
  int is_depth;  // freenect uses different functions based on which kind data is being read
};

unsigned char *depth_to_RGB(unsigned char *data, freenect_frame_mode frame_info) {
  // allocate space for expanded rgb data
  unsigned char* output = malloc(frame_info.width * frame_info.height * 3);
  // for each pixel read the original bits and expand into 24bit RGB
  int bytes_per_pixel = (frame_info.data_bits_per_pixel + frame_info.padding_bits_per_pixel) / 8; for (int16_t y = 0; y < frame_info.height; y++) {
    for (int16_t x = 0; x < frame_info.width; x++) {
      // point to the 11/10 packed/unpacked bits of the next pixel

      int position_offset = x + (y * frame_info.width);
      unsigned char* pixel_data = data + (position_offset * bytes_per_pixel);
      // prase these into a color
      unsigned char r = *pixel_data >> 2;
      unsigned char b = *pixel_data << 4;
      unsigned char g = 0;

      // we can use assignment as chars should always be bytes
      output[(position_offset * 3)] = r;
      output[(position_offset * 3) + 1] = g;
      output[(position_offset * 3) + 2] = b;
    }
  }
  return output;
}

MemoryBuffer *image_to_JPEG(unsigned char *data, freenect_frame_mode frame_info, int is_monochrome) {
  struct jpeg_compress_struct info;
  struct jpeg_error_mgr err;

  unsigned char *lpRowBuffer[1];

  info.err = jpeg_std_error(&err);
  jpeg_CreateCompress(&info, JPEG_LIB_VERSION, sizeof(info));
  uint8_t *outbuffer = NULL;
  uint64_t outlen = 0;
  jpeg_mem_dest(&info, &outbuffer, &outlen);
  info.image_width = frame_info.width;
  info.image_height = frame_info.height;
  if (is_monochrome == TRUE) {
    info.in_color_space = JCS_GRAYSCALE;
    info.input_components = 1;
  }
  else{
    info.in_color_space = JCS_RGB;
    info.input_components = 3;
  }

  jpeg_set_defaults(&info);
  jpeg_set_quality(&info, 99, TRUE);

  jpeg_start_compress(&info, TRUE);
      /* Write everyscanline ... */
  while (info.next_scanline < info.image_height) {
    lpRowBuffer[0] = &(data[info.next_scanline * (frame_info.width * info.input_components)]);
    jpeg_write_scanlines(&info, lpRowBuffer, 1);
  }

  jpeg_finish_compress(&info);

  jpeg_destroy_compress(&info);

  MemoryBuffer* mem = malloc(sizeof(MemoryBuffer));
  mem->buffer = outbuffer;
  mem->size = outlen;
  return mem;
}

// connection callback, continuously serves frames and boundaries until the connection is severed
static ssize_t content_reader(void *cls, uint64_t pos, char *buf, size_t max) {

  struct connection_state *conn = (struct connection_state *)cls;

  if (conn->state == FINISHED) return 0;
  // First phase - send frame boundary and prepare next frame to be served
  if (conn->state == SEND_HEADER) {
    printf("SENDING FRAME HEADER\n");
    // not technically a header but an interframe string
    const char *header =
        "\r\n--" BOUNDARY
        "\r\n"
        "Content-Type: image/jpeg\r\n\r\n";
    // write the header in a char array
    char header_buf[256];
    int header_len = snprintf(header_buf, sizeof(header_buf), header);
    //printf(header_buf);
    //check if header is too big for this connection frame
    // write what can be written to connection buffer
    size_t to_copy = header_len - conn->offset;
    if (to_copy > max) to_copy = max;
    memcpy(buf, header_buf + conn->offset, to_copy);
    conn->offset += to_copy;

    if (conn->offset >= header_len) {
      conn->state = SEND_DATA;
      conn->offset = 0;
      printf("CREATING JPEG\n");
      // Create a JPEG image from the RGB data
      unsigned char *data;
      unsigned int timestamp;
      if (conn->is_depth==0){
        freenect_sync_get_video_with_res((void **)(&data), &timestamp, 0, conn->frame_info.resolution, conn->frame_info.video_format);
      }
      else{
        freenect_sync_get_depth_with_res((void **)(&data), &timestamp, 0, conn->frame_info.resolution, conn->frame_info.depth_format);
        data = depth_to_RGB(data, conn->frame_info);
      }
      conn->mem = image_to_JPEG(data, conn->frame_info, conn->is_monochrome);
      // depth_to_RGB allocates a new memory segment that has to be freed
      if (conn->is_depth == 1){
        free(data);
      }
      // the one produced by freenect is onlyn a poimnter to an internally managed ring buffer, so there's no need to clear it.
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
  // Reject connections outisde knwon URLs
  if (strcmp("/rgb", url) && strcmp("/depth", url) && strcmp("/feed_rgb", url) && strcmp("/feed_depth", url) && strcmp("/feed_ir", url)) {
    return MHD_NO;
  }
  // MJPEG feed request
  if (strncmp("/feed_", url, 6) == 0) {
    // Check state of connection
    if (*con_cls == NULL) {
      // First call - initialize connection state
      struct connection_state *state = malloc(sizeof(*state));
      state->state = SEND_HEADER;
      state->offset = 0;
      *con_cls = state;
      // Set information regarding Kinect data and JPEG settings
      if (strcmp("/feed_rgb", url) == 0) {
        state->frame_info = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
        state->is_depth=0;
        state->is_monochrome = FALSE;
      }
      if (strcmp("/feed_ir", url) == 0) {
        state->frame_info = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_8BIT);
        state->is_depth = 0;
        state->is_monochrome = TRUE;
      }
      if (strcmp("/feed_depth", url) == 0) {
        state->frame_info = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_10BIT);
        state->is_depth=1;
        state->is_monochrome = FALSE;
      }
      return MHD_YES;
    }
    // Second call - preparations complete, run callback and pass info along
    printf("STARTING RESPONSE\n");
    // Create streaming response
    struct MHD_Response *response = MHD_create_response_from_callback(
        MHD_SIZE_UNKNOWN, 4096,
        &content_reader,
        *con_cls,
        &free_connection_state);
    // Prepare and submit first set of headers
    MHD_add_response_header(response, "Content-Type", CONTENT_TYPE);
    // MHD_add_response_header(response, "Cache-Control", "no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0");
    // MHD_add_response_header(response, "Pragma", "no-cache");
    // MHD_add_response_header(response, "Connection", "close");
    // MHD_add_response_header(response, "Content-Length", "1921600");

    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
  }

  // Serve individual frames

  unsigned char *data;
  int output_bit_depth;
  unsigned int timestamp;
  // gather data and parameters based on request 
  freenect_frame_mode frame_info;
  if (strcmp("/rgb", url) == 0) {
      frame_info = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
      freenect_sync_get_video_with_res((void **)(&data), &timestamp, 0, frame_info.resolution, frame_info.video_format);
      output_bit_depth = 8;
    }
  if (strcmp("/depth", url) == 0) {
    frame_info = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_10BIT);
    freenect_sync_get_depth_with_res((void**)&data, &timestamp, 0, frame_info.resolution, frame_info.depth_format);
    output_bit_depth = 16;
    data = depth_to_RGB(data, frame_info);
  }
  printf("Resolution: %dx%d\nBits per Pixel:%d+%d\n", frame_info.width, frame_info.height, frame_info.data_bits_per_pixel, frame_info.padding_bits_per_pixel);
  printf("Total bytes: %d %d\n", frame_info.bytes, (frame_info.width) * (frame_info.height) * (frame_info.data_bits_per_pixel + frame_info.padding_bits_per_pixel) / 8);
  // Create a JPEG image from the RGB data
  MemoryBuffer* mem = image_to_JPEG(data, frame_info, output_bit_depth);

  struct MHD_Response *response;
  int ret;

  response = MHD_create_response_from_buffer(mem->size,
                                             (void *)mem->buffer, MHD_RESPMEM_MUST_FREE);
  MHD_add_response_header(response, "Content-Type", "image/jpeg");
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
