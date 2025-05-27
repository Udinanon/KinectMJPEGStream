#include <libfreenect/libfreenect_sync.h>
#include <microhttpd.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8888

enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                     const char *url,
                                     const char *method, const char *version,
                                     const char *upload_data,
                                     long unsigned int *upload_data_size, void **con_cls) {
  
  int n;
  unsigned long fileLen;
  FILE *fptr;

  if ((fptr = fopen("image.jpeg", "rb")) == NULL) {
    printf("Error! opening file");

    // Program exits if the file pointer returns NULL.
    return 1;
  }

  // Get file length
  fseek(fptr, 0, SEEK_END);
  fileLen = ftell(fptr);
  fseek(fptr, 0, SEEK_SET);

  // Allocate memory
  unsigned char *buffer;
  buffer = (char *)malloc(fileLen);
  if (!buffer) {
    fprintf(stderr, "Memory error!");
    fclose(fptr);
    return 1;
  }

  fread(buffer, 1, fileLen, fptr);

  fclose(fptr);
  char str[1024];  // just big enough

  snprintf(str, sizeof str, "%u", fileLen);

  char *data;
  unsigned int timestamp;
  freenect_sync_get_video((void **)(&data), &timestamp, 0, FREENECT_VIDEO_RGB);
  char* page;
  asprintf(&page, "<html><body>Hello, browser! Timestamp= %u </body></html>", timestamp);

  struct MHD_Response *response;
  int ret;
  response = MHD_create_response_from_buffer(strlen(page),
                                             (void *)page, MHD_RESPMEM_PERSISTENT);
  //MHD_add_response_header(response, "Content-Type", "image/png");
  //response = MHD_create_response_from_buffer(640*480*3,
  //                                           (void *)data, MHD_RESPMEM_PERSISTENT);
  //MHD_add_response_header(response, "Content-Type", "image/png");
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
