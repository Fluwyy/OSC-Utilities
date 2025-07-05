#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <time.h>

#define DEFAULT_RATE_LIMIT_COUNT 2
#define DEFAULT_RATE_LIMIT_SECONDS 1

typedef struct {
    int rateLimitCount;              // Minimum count difference required
    int rateLimitSeconds;            // Minimum time difference required
    int lastExecutionCount;          // Count when action was last executed
    time_t lastExecutionTime;        // Time when action was last executed
} RateLimiter;

// Rate limiter functions
void initRateLimiter(RateLimiter* limiter);
void initRateLimiterWithValues(RateLimiter* limiter, int count, int seconds);
int canExecuteWithRateLimit(RateLimiter* limiter, int currentCount, int enableDebug);
void updateRateLimiterExecution(RateLimiter* limiter, int currentCount);
void resetRateLimiter(RateLimiter* limiter);

// Configuration functions
void setRateLimitValues(RateLimiter* limiter, int count, int seconds);
void getRateLimitValues(const RateLimiter* limiter, int* count, int* seconds);

// Utility functions
const char* formatRateLimitString(const RateLimiter* limiter, char* buffer, size_t bufferSize);
int isRateLimitDefault(const RateLimiter* limiter);

#endif
