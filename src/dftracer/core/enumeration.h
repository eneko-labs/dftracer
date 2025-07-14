//
// Created by haridev on 3/28/23.
//

#ifndef DFTRACER_ENUMERATION_H
#define DFTRACER_ENUMERATION_H
enum WriterType : uint8_t {
  PERFETTO_CHROME_FILE = 0,
  PERFETTO_CHROME_ZMQ = 1,
  PERFETTO_PROTO_FILE = 2,
  PERFETTO_PROTO_ZMQ = 3,
};
enum ProfilerStage : uint8_t {
  PROFILER_INIT = 0,
  PROFILER_FINI = 1,
  PROFILER_OTHER = 2
};
enum ProfileType : uint8_t {
  PROFILER_PRELOAD = 0,
  PROFILER_PY_APP = 1,
  PROFILER_CPP_APP = 2,
  PROFILER_C_APP = 3,
  PROFILER_ANY = 4
};
enum ProfileInitType : uint8_t {
  PROFILER_INIT_NONE = 0,
  PROFILER_INIT_LD_PRELOAD = 1,
  PROFILER_INIT_FUNCTION = 2
};

inline void convert(const std::string &s, ProfileInitType &type) {
  if (s == "PRELOAD") {
    type = ProfileInitType::PROFILER_INIT_LD_PRELOAD;
  } else if (s == "FUNCTION") {
    type = ProfileInitType::PROFILER_INIT_FUNCTION;
  } else {
    type = ProfileInitType::PROFILER_INIT_NONE;
  }
}

inline void convert(const std::string &s, cpplogger::LoggerType &type) {
  if (s == "DEBUG") {
    type = cpplogger::LoggerType::LOG_DEBUG;
  } else if (s == "INFO") {
    type = cpplogger::LoggerType::LOG_INFO;
  } else if (s == "WARN") {
    type = cpplogger::LoggerType::LOG_WARN;
  } else {
    type = cpplogger::LoggerType::LOG_ERROR;
  }
}

inline void convert(const std::string &s, WriterType &type) {
  if (s == "PERFETTO_CHROME_ZMQ") {
    type = WriterType::PERFETTO_CHROME_ZMQ;
  } else if (s == "PERFETTO_PROTO_FILE") {
    type = WriterType::PERFETTO_PROTO_FILE;
  } else if (s == "PERFETTO_PROTO_ZMQ") {
    type = WriterType::PERFETTO_PROTO_ZMQ;
  } else {
    type = WriterType::PERFETTO_CHROME_FILE;  // Default to CHROME FILE
  }
}

#define METADATA_NAME_PROCESS "PR"
#define METADATA_NAME_PROCESS_NAME "process_name"
#define METADATA_NAME_THREAD_NAME "thread_name"
#define METADATA_NAME_FILE_HASH "FH"
#define METADATA_NAME_HOSTNAME_HASH "HH"
#define METADATA_NAME_STRING_HASH "SH"
#define CUSTOM_METADATA "CM"
#endif  // DFTRACER_ENUMERATION_H
