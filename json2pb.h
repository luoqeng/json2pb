/*
 * Copyright (c) 2015 luoqeng <luoqeng@gmail.com>
 *
 * json2pb is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef __JSON2PB_H__
#define __JSON2PB_H__

#include <string>

#include "json/json.h"

namespace google {
    namespace protobuf {
        class Message;
    }
}

namespace jsonpb {

    int json2pb(google::protobuf::Message &msg, const char *buf, size_t size);
    Json::Value pb2json(const google::protobuf::Message &msg);
}

#endif //__JSON2PB_H__
