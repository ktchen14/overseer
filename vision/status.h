#ifndef VS_STATUS_H
#define VS_STATUS_H

#define vs_except(jump, ...) do { \
  int __e = errno; \
  fprintf(stderr, __VA_ARGS__); \
  errno = __e; \
  goto except##_##jump; \
} while (0)

#define vs_return(retval, ...) do { \
  int __e = errno; \
  fprintf(stderr, __VA_ARGS__); \
  errno = __e; \
  return (retval); \
} while (0)

#define vs_continue(...) do { \
  int __e = errno; \
  fprintf(stderr, __VA_ARGS__); \
  errno = __e; \
  continue; \
} while (0)

#endif /* VS_STATUS_H */
