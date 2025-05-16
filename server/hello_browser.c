#include <microhttpd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define PORT 8888

enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                     const char *url,
                                     const char *method, const char *version,
                                     const char *upload_data,
                                     long unsigned int *upload_data_size, void **con_cls) {
  const char *page = "<html><body>Hello, browser!</body></html>";

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
  
  struct MHD_Response *response;
  int ret;

  response = MHD_create_response_from_buffer(fileLen,
                                             (void *)buffer, MHD_RESPMEM_PERSISTENT);
  MHD_add_response_header(response, "Content-Type", "image/jpeg");
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
