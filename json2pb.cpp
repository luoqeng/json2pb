/*
 * Copyright (c) 2015 luoqeng <luoqeng@gmail.net>
 *
 * json2pb is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <errno.h>
#include "json/json.h"
#include "log/LogWrapper.h"

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>

#include "json2pb.h"

namespace {
#include "bin2ascii.h"
}

//using google::protobuf::google::protobuf::Message;
//using google::protobuf::google::protobuf::MessageFactory;
//using google::protobuf::Descriptor;
//using google::protobuf::google::protobuf::FieldDescriptor;
//using google::protobuf::EnumDescriptor;
//using google::protobuf::EnumValueDescriptor;
//using google::protobuf::google::protobuf::Reflection;

namespace jsonpb{

    static Json::Value _pb2json(const google::protobuf::Message& msg);
    static Json::Value _field2json(const google::protobuf::Message& msg, const google::protobuf::FieldDescriptor *field, size_t index)
    {
        const google::protobuf::Reflection *ref = msg.GetReflection();
        const bool repeated = field->is_repeated();
        Json::Value jf;
        switch (field->cpp_type())
        {
    #define _CONVERT(type, ctype, sfunc, afunc)		\
            case google::protobuf::FieldDescriptor::type: {			\
                ctype value = (repeated)?		\
                    ref->afunc(msg, field, index):	\
                    ref->sfunc(msg, field);		\
                jf = value;\
                break;					\
            }

            _CONVERT(CPPTYPE_BOOL, bool, GetBool, GetRepeatedBool);
            _CONVERT(CPPTYPE_DOUBLE, double, GetDouble, GetRepeatedDouble);
            _CONVERT(CPPTYPE_FLOAT, float, GetFloat, GetRepeatedFloat);
            _CONVERT(CPPTYPE_INT64, Json::Int64, GetInt64, GetRepeatedInt64);
            _CONVERT(CPPTYPE_UINT64, Json::UInt64, GetUInt64, GetRepeatedUInt64);
            _CONVERT(CPPTYPE_INT32, Json::Int, GetInt32, GetRepeatedInt32);
            _CONVERT(CPPTYPE_UINT32, Json::UInt, GetUInt32, GetRepeatedUInt32);
    #undef _CONVERT
            case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                std::string scratch;
                const std::string &value = (repeated)?
                    ref->GetRepeatedStringReference(msg, field, index, &scratch):
                    ref->GetStringReference(msg, field, &scratch);
                if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES)
                    jf = b64_encode(value);
                else
                    jf = value;
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                const google::protobuf::Message& mf = (repeated)?
                    ref->GetRepeatedMessage(msg, field, index):
                    ref->GetMessage(msg, field);
                jf = _pb2json(mf);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
                const google::protobuf::EnumValueDescriptor* ef = (repeated)?
                    ref->GetRepeatedEnum(msg, field, index):
                    ref->GetEnum(msg, field);

                jf = ef->number();
                break;
            }
            default:
                break;
        }
        if (jf.isNull()) WLOGDEBUG("Fail to convert to json is null: %s", field->name().c_str());
        return jf;
    }

    static Json::Value _pb2json(const google::protobuf::Message& msg)
    {
        const google::protobuf::Descriptor *d = msg.GetDescriptor();
        const google::protobuf::Reflection *ref = msg.GetReflection();
        if (!d || !ref) return 0;

        Json::Value value;
        std::vector<const google::protobuf::FieldDescriptor *> fields;
        ref->ListFields(msg, &fields);

        for (size_t i = 0; i != fields.size(); i++)
        {
            const google::protobuf::FieldDescriptor *field = fields[i];

            Json::Value jf;
            if(field->is_repeated()) {
                size_t count = ref->FieldSize(msg, field);
                if (!count) continue;

                for (size_t j = 0; j < count; j++)
                    jf.append(_field2json(msg, field, j));

            } else if (ref->HasField(msg, field))
                jf = _field2json(msg, field, 0);
            else
                continue;

            const std::string &name = (field->is_extension())?field->full_name():field->name();
            value[name] = jf;
        }
        return value;
    }

    static int _json2pb(google::protobuf::Message& msg, const Json::Value &value);
    static int _json2field(google::protobuf::Message &msg, const google::protobuf::FieldDescriptor *field, const Json::Value &value)
    {
        const google::protobuf::Reflection *ref = msg.GetReflection();
        const bool repeated = field->is_repeated();

        switch (field->cpp_type())
        {
    #define _SET_OR_ADD(sfunc, afunc, value)			\
            do {						\
                if (repeated)				\
                    ref->afunc(&msg, field, value);	\
                else					\
                    ref->sfunc(&msg, field, value);	\
            } while (0)

    #define _CONVERT(type, ctype, sfunc, afunc) 		\
            case google::protobuf::FieldDescriptor::type: {			\
                if (!value.is##ctype()) {       \
                    WLOGERROR("Failed to unpack: %s", field->name().c_str()); \
                    return -1;      \
                }       \
                _SET_OR_ADD(sfunc, afunc, value.as##ctype());	\
                break;					\
            }

            _CONVERT(CPPTYPE_DOUBLE, Double, SetDouble, AddDouble);
            _CONVERT(CPPTYPE_FLOAT, Double, SetFloat, AddFloat);
            _CONVERT(CPPTYPE_BOOL, Bool, SetBool, AddBool);
            _CONVERT(CPPTYPE_INT64, Int64, SetInt64, AddInt64);
            _CONVERT(CPPTYPE_UINT64, UInt64, SetUInt64, AddUInt64);
            _CONVERT(CPPTYPE_INT32, Int, SetInt32, AddInt32);
            _CONVERT(CPPTYPE_UINT32, UInt, SetUInt32, AddUInt32);

            case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                if (!value.isString()) {
                    WLOGERROR( "Not a string: %s", field->name().c_str());
                    return -1;
                }
                std::string str = value.asString();
                if(field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES)
                    _SET_OR_ADD(SetString, AddString, b64_decode(str));
                else
                    _SET_OR_ADD(SetString, AddString, str);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                google::protobuf::Message *mf = (repeated)?
                    ref->AddMessage(&msg, field):
                    ref->MutableMessage(&msg, field);
                if(_json2pb(*mf, value)) return -1;
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
                const google::protobuf::EnumDescriptor *ed = field->enum_type();
                const google::protobuf::EnumValueDescriptor *ev = 0;
                if (value.isInt()) {
                    ev = ed->FindValueByNumber(value.asInt());
                } else if (value.isString()) {
                    ev = ed->FindValueByName(value.asString());
                } else {
                    WLOGERROR("Not an integer or string: %s", field->name().c_str());
                    return -1;
                }
                if (!ev) {
                    WLOGERROR("Enum value not found: %s", field->name().c_str());
                    return -1;
                }
                _SET_OR_ADD(SetEnum, AddEnum, ev);
                break;
            }
            default:
                break;

    #undef _SET_OR_ADD
    #undef _CONVERT
        }
        return 0;
    }

    static int _json2pb(google::protobuf::Message& msg, const Json::Value &value)
    {
        const google::protobuf::Descriptor *d = msg.GetDescriptor();
        const google::protobuf::Reflection *ref = msg.GetReflection();
        if (!d || !ref) {
            WLOGERROR("No descriptor or reflection");
            return -1;
        }

        const Json::Value::Members &members = value.getMemberNames();
        for(Json::Value::Members::const_iterator it = members.begin(); it != members.end(); ++it) {

            const std::string &name = *it;
            const google::protobuf::FieldDescriptor *field = d->FindFieldByName(name);
            if (!field)
                field = ref->FindKnownExtensionByName(name);
                //field = d->file()->FindExtensionByName(name);
            if (!field) {
                WLOGERROR("Unknown field: %s\n", name.c_str());
                continue;
            }
            if (value[name].isNull()) {
                //ref->ClearField(msg, field);
                continue;
            }
            if (field->is_repeated() != value[name].isArray()) {
                WLOGERROR("field or value Not array: %s", field->name().c_str());
                return -1;
            }

            if (field->is_repeated()) {
                for (Json::Value::iterator it = value[name].begin(); it != value[name].end(); ++it) {
                   if(_json2field(msg, field, *it)) return -1;
                }
            } else
                if(_json2field(msg, field, value[name])) return -1;
        }

        return 0;
    }

    int json2pb(google::protobuf::Message &msg, const char *buf, size_t size)
    {
        std::string json_msg(buf, size);
        Json::Value value;
        Json::Reader reader;

        if(!reader.parse(json_msg, value)) {
            WLOGERROR("Parse Json_msg failed \n%s", json_msg.c_str());
            return -1;
        }

        WLOGDEBUG("json2pb recv json\n%s", value.toStyledString().c_str());

        return  _json2pb(msg, value);
    }

    Json::Value pb2json(const google::protobuf::Message &msg)
    {
        //Json::FastWriter fastwriter;
        //std::string r;

        Json::Value value = _pb2json(msg);
        // 格式化
        //r = value.toStyledString();

        // 无格式
        //r = fastwriter.write(value);

        return value;
    }

}

