//
// Created by haridev on 10/7/23.
//

#ifndef DFTRACER_TYPEDEF_H
#define DFTRACER_TYPEDEF_H

#ifdef __cplusplus
#include <any>
#include <string>
#include <unordered_map>
using MetadataMap = std::unordered_map<std::string, std::any>;
#endif

typedef unsigned long long int TimeResolution;
typedef unsigned long int ThreadID;
typedef unsigned long int ProcessID;
typedef char* EventNameType;
typedef const char* ConstEventNameType;
#if DFTRACER_HASHING_ENABLE
typedef char* HashType;
#else
typedef const char* HashType;
#endif

#endif  // DFTRACER_TYPEDEF_H
