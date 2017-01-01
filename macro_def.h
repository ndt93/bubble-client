#define STRUCT_MEMBER_POS(t,m)  ((unsigned long)(&(((t *)(0))->m)))

#define LOG_ERR(msg) (std::fprintf(stderr, "[ERROR] %s\n", (msg)))
#define LOG_WARN(msg) (std::fprintf(stderr, "[WARN] %s\n", (msg)))
#define LOG_BUFFER(msg, nbytes, buffer) (std::printf("%s\n%.*s\n", (msg), (int)(nbytes), (buffer)))
