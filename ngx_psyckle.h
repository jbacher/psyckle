#ifndef PSYCKLE_H
#define PSYCKLE_H

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
  #include <ngx_http.h>
}

#define NGX_HTTP_IMAGE_READ 0 
#define NGX_HTTP_IMAGE_PROCESS 1
#define NGX_HTTP_IMAGE_FINISHED 2
#define NGX_HTTP_IMAGE_BUFFERED  0x08

#define IMAGE_ACTION_RESIZE 1

#define IMAGE_DEFAULT_WIDTH 100
#define IMAGE_DEFAULT_HEIGHT 100

typedef struct {
    ngx_uint_t  width;
    ngx_uint_t  height;
    ngx_uint_t  action;
} ngx_http_psyckle_conf_t;

typedef struct {
    ngx_uint_t  width;
    ngx_uint_t  height;
    ngx_uint_t  action;
} ngx_http_psyckle_image_specs;

typedef struct {
    size_t              length;
    u_char*             image_buffer;
    u_char*             image_buffer_end;
    std::vector<u_char> finished_image_vector;
    u_char*             finished_image_buffer;
    size_t              finished_image_size;
    size_t              image_size;
    ngx_uint_t          phase;
} ngx_http_psyckle_ctx_t;

ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
ngx_http_output_body_filter_pt    ngx_http_next_body_filter;

ngx_str_t  ngx_http_image_types[] = {
    ngx_string("image/jpeg"),
    ngx_string("image/gif"),
    ngx_string("image/png")
};

ngx_str_t ngx_http_psyckle_param_width = ngx_string("width");
ngx_str_t ngx_http_psyckle_param_height = ngx_string("height");
ngx_str_t ngx_http_psyckle_param_action = ngx_string("action");

void* ngx_http_psyckle_create_conf(ngx_conf_t*);
char* ngx_http_psyckle_merge_conf(ngx_conf_t*, void*, void*);

ngx_int_t ngx_http_psyckle_init(ngx_conf_t *cf);
ngx_int_t ngx_http_psyckle_header_filter(ngx_http_request_t *r);
ngx_int_t ngx_http_psyckle_body_filter(ngx_http_request_t*, ngx_chain_t*);
ngx_int_t ngx_http_psyckle_filter_init(ngx_conf_t*);
ngx_int_t ngx_http_psyckle_read_image(ngx_http_request_t*, ngx_chain_t*, ngx_http_psyckle_ctx_t*, ngx_http_psyckle_conf_t*);
ngx_int_t ngx_http_psyckle_send_image(ngx_http_request_t*, ngx_chain_t*, ngx_http_psyckle_ctx_t*, ngx_http_psyckle_conf_t*);

void ngx_http_psyckle_set_headers_out(ngx_http_request_t*, ngx_http_psyckle_ctx_t*, ngx_http_psyckle_conf_t*); 

ngx_uint_t ngx_http_psyckle_conf_num_value(ngx_str_t*);
ngx_uint_t ngx_http_psyckle_cross_multiply(ngx_uint_t, ngx_uint_t, ngx_uint_t);

ngx_http_psyckle_image_specs ngx_http_psyckle_get_image_specs(ngx_http_request_t*, ngx_http_psyckle_ctx_t*, ngx_http_psyckle_conf_t*); 
ngx_buf_t* ngx_http_psyckle_process_image(ngx_http_request_t*, ngx_http_psyckle_image_specs, ngx_http_psyckle_ctx_t*, ngx_http_psyckle_conf_t*);

//OpenCV specific operations
cv::Mat ngx_http_resize_image(cv::Mat image_buf, ngx_uint_t width, ngx_uint_t height); 

char* ngx_psyckle(ngx_conf_t*, ngx_command_t*, void*);

ngx_http_psyckle_ctx_t* ngx_http_psyckle_get_module_ctx(ngx_http_request_t*, ngx_module_t);
ngx_http_psyckle_conf_t* ngx_http_psyckle_get_module_loc_conf(ngx_http_request_t*, ngx_module_t);

ngx_command_t  ngx_http_psyckle_commands[] = {

    { ngx_string("psyckle"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_psyckle,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};

ngx_http_module_t  ngx_http_psyckle_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_psyckle_init,                 /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_psyckle_create_conf,         /* create location configuration */
    ngx_http_psyckle_merge_conf           /* merge location configuration */
};


ngx_module_t  ngx_http_psyckle_module = {
    NGX_MODULE_V1,
    &ngx_http_psyckle_module_ctx,     /* module context */
    ngx_http_psyckle_commands,        /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

#endif
