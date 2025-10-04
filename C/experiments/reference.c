// std lib includes
#include <stdio.h>
#include <string.h>
#include <sstream>

// HTTP server includes
#include "platform.h"
#include "microhttpd.h"

//! Buffer chunk size for \a POST data loading - not too big, not too small.
#define POSTBUFFERSIZE  2048

//! HTTP method \a GET.
#define GET             0
//! HTTP method \a POST.
#define POST            1

//! Structure holding all connection related data.
struct connection_info_struct
{
    int connection_type;                  //!< \a GET or \a POST.
    MHD_PostProcessor* post_processor;    //!< Processor for chunks of \a POST data.
    std::string data;                     //!< Loaded \a POST data.

    connection_info_struct()
    {
        connection_type = GET;
        post_processor = NULL;
    }

    ~connection_info_struct()
    {
        if (NULL != post_processor)
            MHD_destroy_post_processor(post_processor);
        post_processor = NULL;
    }
};

int send_page(MHD_Connection* connection, const char* page, int status_code)
{
    MHD_Response* response;
    response = MHD_create_response_from_data(strlen(page),
        (void*) page, MHD_NO, MHD_YES);
    if (!response)
        return MHD_NO;

    int ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

int on_client_connect(void* cls, const sockaddr* addr, socklen_t addrlen)
{
    //! \todo Here you can do any statistics, IP filtering, etc.
    char buf[INET6_ADDRSTRLEN];
    printf("Incoming IP: %s\n", inet_ntop(addr->sa_family,
        addr->sa_data + 2, buf, INET6_ADDRSTRLEN));
    //! \todo Return MHD_NO if you want to refuse the connection.
    return MHD_YES;
}

int iterate_post(void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
                 const char *filename, const char *content_type,
                 const char *transfer_encoding, const char *data, uint64_t off,
                 size_t size)
{
    connection_info_struct* con_info = (connection_info_struct*) coninfo_cls;

    /*! \todo: This is where we want to do the processing of POST data.
     * Here we simply append it to \a data member for further use. */
    if (size > 0)
        con_info->data += std::string(data, size);

    return MHD_YES;
}

int answer_to_connection(void* cls, MHD_Connection* connection,
                         const char* url, const char* method,
                         const char* version, const char* upload_data,
                         size_t* upload_data_size, void** con_cls)
{
    if (NULL == *con_cls)    // Incoming!... prepare all.
    {
        connection_info_struct* con_info = new connection_info_struct;
        if (NULL == con_info)
            return MHD_NO;

        if (0 == strcmp(method, "POST"))
        {
            con_info->post_processor =
                MHD_create_post_processor(connection, POSTBUFFERSIZE,
                iterate_post, (void*) con_info);

            if (NULL == con_info->post_processor)
            {
                delete con_info;
                return MHD_NO;
            }
            con_info->connection_type = POST;
        }
        else
            con_info->connection_type = GET;

        *con_cls = (void*) con_info;

        return MHD_YES;
    }

    if (0 == strcmp(method, "GET"))
    {
        //! \todo Send an appropriate reply to GET request, based on \a url.
        return send_page(connection, "Some reply...", MHD_HTTP_OK);
    }

    if (0 == strcmp(method, "POST"))
    {
        //! \todo Process a POST request.
        connection_info_struct* con_info = (connection_info_struct*) *con_cls;

        if (0 != *upload_data_size)        // chunk of POST data?
        {
            // we're still receiving the POST data, process them.
            MHD_post_process(con_info->post_processor,
                upload_data, *upload_data_size);
            *upload_data_size = 0;    // 0 means "successfully processed"
            return MHD_YES;
        }
        else    // awaiting reply
        {
            //! \todo Send an appropriate reply to POST request, based on \a url.
            return send_page(connection, "Some reply...", MHD_HTTP_OK);
        }
    }
    return MHD_YES;
}

void request_completed(void* cls, MHD_Connection* connection,
                       void** con_cls, MHD_RequestTerminationCode toe)
{
    //! \todo Here we do the clean up on connection close.
    connection_info_struct* con_info = (connection_info_struct*) *con_cls;
    if (NULL != con_info)
        delete con_info;
    con_info = NULL;
    *con_cls = NULL;
}

int main(int argc, char** argv)
{
    int port = 1234;
    MHD_Daemon* daemon = MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION, port,
        on_client_connect, NULL,
        &answer_to_connection, NULL,
        MHD_OPTION_NOTIFY_COMPLETED, request_completed,
        NULL, MHD_OPTION_END);
    if (NULL == daemon)
        return 1;

    //! \todo Here we just sleep... you can play chess here, or whatever you want.
    while (stop_condition)
        usleep(100000);    // sleep 0.1 sec

    MHD_stop_daemon(daemon);
    return 0;
}