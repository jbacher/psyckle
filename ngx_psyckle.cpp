#include "ngx_psyckle.h"

ngx_http_psyckle_ctx_t* ngx_http_psyckle_get_module_ctx(ngx_http_request_t *r, ngx_module_t module) {
  return reinterpret_cast<ngx_http_psyckle_ctx_t*>(ngx_http_get_module_ctx(r, module));
}

ngx_http_psyckle_conf_t* ngx_http_psyckle_get_module_loc_conf(ngx_http_request_t *r, ngx_module_t module) {
  return reinterpret_cast<ngx_http_psyckle_conf_t*>(ngx_http_get_module_loc_conf(r, module));
}

ngx_int_t
ngx_http_psyckle_read_image(ngx_http_request_t *r, ngx_chain_t *in, ngx_http_psyckle_ctx_t *ctx, ngx_http_psyckle_conf_t *conf) {
    u_char          *p;
    size_t          size, rest;
    ngx_buf_t       *b;
    ngx_chain_t     *cl;

    if (ctx->image_buffer == NULL) {
        ctx->image_buffer = static_cast<u_char*>(ngx_pcalloc(r->pool, ctx->length));
        ctx->image_buffer_end = ctx->image_buffer;
        if (ctx->image_buffer == NULL) {
            return NGX_ERROR;
        }
    }

    p = ctx->image_buffer_end;

    for (cl = in; cl; cl = cl->next) {
        b = cl->buf;
        size = b->last - b->pos;
        ctx->image_size += size;

        rest = ctx->image_buffer  + ctx->length - p;

        if (size > rest) {
            return NGX_ERROR;
        }

        p = ngx_cpymem(p, b->pos, size);
        b->pos += size;
        
        if (b->last_buf) {
            ctx->image_buffer_end = p;
            //Well, we had a problem.
            if (ctx->image_size == 0) {
                return NGX_ERROR;
            }
            return NGX_OK;
        }
    } 

    ctx->image_buffer_end = p;

    //Have not figured out why we need this yet. Signals something?
    //Maybe: This connection has some buffer already, don't ovewrite?
    //No idea.
    r->connection->buffered |= NGX_HTTP_IMAGE_BUFFERED;

    return NGX_AGAIN;
}

ngx_http_psyckle_image_specs
ngx_http_psyckle_get_image_specs(ngx_http_request_t *r, ngx_http_psyckle_ctx_t *ctx, ngx_http_psyckle_conf_t *conf) {
    ngx_http_psyckle_image_specs specs;
    ngx_str_t   width;
    ngx_str_t   height;
    ngx_str_t   action;
    ngx_uint_t  num_width;
    ngx_uint_t  num_height;
    
    ngx_memzero(static_cast<void*>(&specs), sizeof(specs));
    
    //Default values
    specs.action = conf->action;

    ngx_memzero(static_cast<void*>(&width), sizeof(ngx_str_t));
    ngx_memzero(static_cast<void*>(&height), sizeof(ngx_str_t));
    ngx_memzero(static_cast<void*>(&action), sizeof(ngx_str_t));

    ngx_http_arg(r, ngx_http_psyckle_param_width.data, ngx_http_psyckle_param_width.len, &width);
    ngx_http_arg(r, ngx_http_psyckle_param_height.data, ngx_http_psyckle_param_height.len, &height);
    ngx_http_arg(r, ngx_http_psyckle_param_action.data, ngx_http_psyckle_param_action.len, &action);

    //If we have width/height, set it accordingly,
    //else set it to 0.
    if (width.len > 0) {
        num_width = ngx_atoi(width.data, width.len);     
    } else {
        num_width = 0;
    }

    if (height.len > 0) {
        num_height = ngx_atoi(height.data, height.len);
    } else {
        num_height = 0;
    }

    //If everything is 0, use our defaults.
    if (num_height == 0 && num_width == 0) {
        specs.width = conf->width;
        specs.height = conf->height; 
    } else {
        specs.width = num_width;
        specs.height = num_height;
    }

    return specs;
}

//Feels like we're gonna get real complicated real fast.
ngx_buf_t*
ngx_http_psyckle_process_image(ngx_http_request_t *r, ngx_http_psyckle_image_specs specs,  ngx_http_psyckle_ctx_t *ctx, ngx_http_psyckle_conf_t *conf) {
    ngx_buf_t           *out;
    std::vector<uchar>  buf;

    buf.assign(ctx->image_buffer, ctx->image_buffer + ctx->image_size);

    r->connection->buffered &= ~NGX_HTTP_IMAGE_BUFFERED;

    //This feels like it's gonna be dicey. We went from u_char* to
    //std::vector<u_char> to Mat. That's a lot of converting...
    cv::Mat image_buf = cv::imdecode(cv::Mat(buf), 1);

    if (image_buf.empty()) {
        return NULL;
    }

    out = reinterpret_cast<ngx_buf_t*>(ngx_pcalloc(r->pool, sizeof(ngx_buf_t)));

    if (out == NULL) {
        return NULL;
    }

    cv::Mat resized_image = ngx_http_resize_image(image_buf, specs.width, specs.height);
    cv::imencode(".jpg", resized_image, ctx->finished_image_vector);
    ctx->finished_image_buffer = ctx->finished_image_vector.data();
    ctx->finished_image_size = ctx->finished_image_vector.size();

    out->pos = ctx->finished_image_buffer;
    out->last = ctx->finished_image_buffer + ctx->finished_image_size;
    out->memory = 1;
    out->last_buf = 1;

    return out;
}

cv::Mat
ngx_http_resize_image(cv::Mat image_buf, ngx_uint_t width, ngx_uint_t height) {
    if (width == 0) {
        width = ngx_http_psyckle_cross_multiply(image_buf.size().height, height, image_buf.size().height);
    } else if (height == 0) {
        height = ngx_http_psyckle_cross_multiply(image_buf.size().height, width, image_buf.size().width);
    }

    cv::Mat resized_image(height, width, CV_8UC1);
    cv::resize(image_buf, resized_image, resized_image.size(), 0, 0, cv::INTER_LINEAR);

   return resized_image; 
}

//Formula is:
//width = width' (width_p)
//height = height' (height_p)
//So cross multiply to find the missing value
//Assume we're always going to get width height (we're just looking for a missing prime).
ngx_uint_t
ngx_http_psyckle_cross_multiply(ngx_uint_t val1, ngx_uint_t val2, ngx_uint_t val3) {
    return (val1 * val2) / val3;
}

ngx_int_t
ngx_http_psyckle_send_image(ngx_http_request_t *r, ngx_chain_t *in, ngx_http_psyckle_ctx_t *ctx, ngx_http_psyckle_conf_t *conf) {
    ngx_int_t rc;

    ngx_http_psyckle_set_headers_out(r, ctx, conf);
    
    rc = ngx_http_next_header_filter(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return NGX_ERROR;
    }

    rc = ngx_http_next_body_filter(r, in);

    return rc;
}


ngx_int_t
ngx_http_psyckle_header_filter(ngx_http_request_t *r) {
    //off_t                          len;
    //ngx_http_psyckle_conf_t        *conf;
    ngx_http_psyckle_ctx_t  *ctx;    

    if (r->headers_out.status == NGX_HTTP_NOT_MODIFIED) {
        return ngx_http_next_header_filter(r);
    }

    ctx = ngx_http_psyckle_get_module_ctx(r, ngx_http_psyckle_module);

    if (ctx) {
        ngx_http_set_ctx(r, NULL, ngx_http_psyckle_module);
        return ngx_http_next_header_filter(r);
    } 

    ctx = static_cast<ngx_http_psyckle_ctx_t*>(ngx_pcalloc(r->pool, sizeof(ngx_http_psyckle_ctx_t)));

    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(r, ctx, ngx_http_psyckle_module);

    //When is content_length unknown?
    if (r->headers_out.content_length_n == -1) {
        ctx->length = 1024 * 1024 * 1;
    } else {
        ctx->length = (size_t) r->headers_out.content_length_n;
    }

    if (r->headers_out.refresh) {
        r->headers_out.refresh->hash = 0;
    }

    r->main_filter_need_in_memory = 1; //no idea
    r->allow_ranges = 0; //no range requests for you. 

    return NGX_OK;
    //return ngx_http_next_header_filter(r);
}


ngx_int_t
ngx_http_psyckle_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {
    ngx_http_psyckle_conf_t         *conf;
    ngx_http_psyckle_ctx_t          *ctx;
    ngx_int_t                       status;
    ngx_chain_t                     out;
    ngx_http_psyckle_image_specs    specs;

    if (in == NULL) {
        return ngx_http_next_body_filter(r, in);
    }

    conf = ngx_http_psyckle_get_module_loc_conf(r, ngx_http_psyckle_module);
    ctx = ngx_http_psyckle_get_module_ctx(r, ngx_http_psyckle_module);

    if (ctx == NULL) {
        return ngx_http_next_body_filter(r, in);
    }

    switch(ctx->phase) {
        case NGX_HTTP_IMAGE_READ:
            status = ngx_http_psyckle_read_image(r, in, ctx, conf);

            if (status == NGX_AGAIN) {
                return NGX_OK;
            }

            if (status != NGX_ERROR) {
                ctx->phase = NGX_HTTP_IMAGE_PROCESS;
            } else {
                ctx->phase = NGX_HTTP_IMAGE_FINISHED;
                goto fullstop;
            }

        case NGX_HTTP_IMAGE_PROCESS:
            specs = ngx_http_psyckle_get_image_specs(r, ctx, conf);
            out.buf = ngx_http_psyckle_process_image(r, specs, ctx, conf);

            if (out.buf == NULL) {
                ctx->phase = NGX_HTTP_IMAGE_FINISHED;
            }

            out.next = NULL;

            ctx->phase = NGX_HTTP_IMAGE_FINISHED;

            return ngx_http_psyckle_send_image(r, &out, ctx, conf); 

        case NGX_HTTP_IMAGE_FINISHED:
            return ngx_http_next_body_filter(r, in);

    //Eat your heart out dijkstra
    fullstop:
        default:
           status = ngx_http_next_body_filter(r, NULL);

           //reset any pending data. Hilarious.
           return (status == NGX_OK) ? NGX_ERROR : status;

    }

    return ngx_http_next_body_filter(r, in);
}

void
ngx_http_psyckle_set_headers_out(ngx_http_request_t* r, ngx_http_psyckle_ctx_t *ctx, ngx_http_psyckle_conf_t *conf) {
    r->headers_out.content_type_len = ngx_http_image_types[0].len;
    r->headers_out.content_type = ngx_http_image_types[0];
    r->headers_out.content_type_lowcase = NULL;

    r->headers_out.content_length_n = ctx->finished_image_size;

    if (r->headers_out.content_length) {
        r->headers_out.content_length->hash = 0;
    }

    r->headers_out.content_length = NULL; 
}

void*
ngx_http_psyckle_create_conf(ngx_conf_t *cf) {
    ngx_http_psyckle_conf_t  *conf;

    conf = static_cast<ngx_http_psyckle_conf_t*>(ngx_pcalloc(cf->pool, sizeof(ngx_http_psyckle_conf_t)));

    conf->action = NGX_CONF_UNSET_UINT;
    conf->width = NGX_CONF_UNSET_UINT;
    conf->height = NGX_CONF_UNSET_UINT;

    if (conf == NULL) {
        return NULL;
    }

    return conf;
}

char*
ngx_http_psyckle_merge_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_psyckle_conf_t *prev = static_cast<ngx_http_psyckle_conf_t*>(parent);
    ngx_http_psyckle_conf_t *conf = static_cast<ngx_http_psyckle_conf_t*>(child);

    if (prev->action == NGX_CONF_UNSET_UINT) {
        if (conf->action == NGX_CONF_UNSET_UINT) {
            conf->action = IMAGE_ACTION_RESIZE;
            conf->width = IMAGE_DEFAULT_WIDTH;
            conf->height = IMAGE_DEFAULT_HEIGHT;
        }
    } else {
        conf->action = NGX_CONF_UNSET_UINT;
        conf->width = IMAGE_DEFAULT_WIDTH;
        conf->height = IMAGE_DEFAULT_HEIGHT;
    }
    

    return NGX_CONF_OK;
}

char* ngx_psyckle(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_psyckle_conf_t *pycf = static_cast<ngx_http_psyckle_conf_t*>(conf);

    ngx_str_t   *value;
    ngx_uint_t  num;

    value = static_cast<ngx_str_t*>(cf->args->elts);

    pycf->action = IMAGE_ACTION_RESIZE;
    pycf->width = IMAGE_DEFAULT_WIDTH;
    pycf->height = IMAGE_DEFAULT_HEIGHT;

    //Default to and check for resize
    if (cf->args->nelts > 1) {
        if (ngx_strcmp(value[1].data, "resize") == 0) {
            if (cf->args->nelts > 2) {
                num = ngx_http_psyckle_conf_num_value(&value[2]);

                if (num > 0) {
                    pycf->width = num;
                }
            }

            if (cf->args->nelts > 3) {
                num = ngx_http_psyckle_conf_num_value(&value[3]);
                
                if (num > 0) {
                    pycf->height = num;
                }
            }
        }
    }

    return NGX_CONF_OK;
}

ngx_uint_t
ngx_http_psyckle_conf_num_value(ngx_str_t *val) {
    ngx_uint_t  num;

    //Negative values not allowed.
    if (val->len > 0 && val->data[0] == '0') {
        return -1;
    }

    num = ngx_atoi(val->data, val->len);

    return num;
}

ngx_int_t
ngx_http_psyckle_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_psyckle_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_psyckle_body_filter;

    return NGX_OK;
}
