#include "rateLimiter.h"
#include <stdio.h>
#include <string.h>

void initRateLimiter(RateLimiter* limiter) {
    if (!limiter) return;
    
    limiter->rateLimitCount = DEFAULT_RATE_LIMIT_COUNT;
    limiter->rateLimitSeconds = DEFAULT_RATE_LIMIT_SECONDS;
    limiter->lastExecutionCount = 0;
    limiter->lastExecutionTime = 0;
}

void initRateLimiterWithValues(RateLimiter* limiter, int count, int seconds) {
    if (!limiter) return;
    
    limiter->rateLimitCount = (count > 0) ? count : DEFAULT_RATE_LIMIT_COUNT;
    limiter->rateLimitSeconds = (seconds >= 0) ? seconds : DEFAULT_RATE_LIMIT_SECONDS;
    limiter->lastExecutionCount = 0;
    limiter->lastExecutionTime = 0;
}

int canExecuteWithRateLimit(RateLimiter* limiter, int currentCount, int enableDebug) {
    if (!limiter) return 0;
    
    time_t currentTime = time(NULL);
    
    int countDiff = currentCount - limiter->lastExecutionCount;
    int countOK = (countDiff >= limiter->rateLimitCount);
    
    int timeOK = 0;
    double timeDiff = 0.0;
    
    if (limiter->lastExecutionTime == 0) {
        timeOK = 1; 
        timeDiff = 0.0;
    } else {
        timeDiff = difftime(currentTime, limiter->lastExecutionTime);
        timeOK = (timeDiff >= limiter->rateLimitSeconds);
    }
    
    if (enableDebug) {
        printf("Rate limit check: Count diff=%d (need >=%d, %s), Time diff=%.1fs (need >=%ds, %s)\n",
               countDiff, limiter->rateLimitCount, countOK ? "OK" : "BLOCKED",
               timeDiff, limiter->rateLimitSeconds, timeOK ? "OK" : "BLOCKED");
    }
    
    if (limiter->rateLimitCount <= 1 && limiter->rateLimitSeconds <= 0) {
        
        return 1;
    }
    
    return (countOK && timeOK);
}

void updateRateLimiterExecution(RateLimiter* limiter, int currentCount) {
    if (!limiter) return;
    
    limiter->lastExecutionCount = currentCount;
    limiter->lastExecutionTime = time(NULL);
}

void resetRateLimiter(RateLimiter* limiter) {
    if (!limiter) return;
    
    limiter->lastExecutionCount = 0;
    limiter->lastExecutionTime = 0;
}

void setRateLimitValues(RateLimiter* limiter, int count, int seconds) {
    if (!limiter) return;
    
    if (count < 1) {
        printf("Warning: Count must be at least 1, using default (%d)\n", DEFAULT_RATE_LIMIT_COUNT);
        count = DEFAULT_RATE_LIMIT_COUNT;
    }
    
    if (seconds < 0) {
        printf("Warning: Seconds must be 0 or greater, using default (%d)\n", DEFAULT_RATE_LIMIT_SECONDS);
        seconds = DEFAULT_RATE_LIMIT_SECONDS;
    }
    
    limiter->rateLimitCount = count;
    limiter->rateLimitSeconds = seconds;
}

void getRateLimitValues(const RateLimiter* limiter, int* count, int* seconds) {
    if (!limiter || !count || !seconds) return;
    
    *count = limiter->rateLimitCount;
    *seconds = limiter->rateLimitSeconds;
}

const char* formatRateLimitString(const RateLimiter* limiter, char* buffer, size_t bufferSize) {
    if (!limiter || !buffer || bufferSize == 0) return "";
    
    snprintf(buffer, bufferSize, "%dc/%ds", 
             limiter->rateLimitCount, limiter->rateLimitSeconds);
    return buffer;
}

int isRateLimitDefault(const RateLimiter* limiter) {
    if (!limiter) return 0;
    
    return (limiter->rateLimitCount == DEFAULT_RATE_LIMIT_COUNT && 
            limiter->rateLimitSeconds == DEFAULT_RATE_LIMIT_SECONDS);
}
