#ifndef KEY_PRESS_H
#define KEY_PRESS_H

#include <linux/input.h>
#include <linux/uinput.h>

// Hash table configuration
#define KEY_HASH_TABLE_SIZE 256
#define MAX_KEY_NAME_LENGTH 32

// Key press action types
typedef enum {
    KEY_ACTION_SINGLE,      // Single key press
    KEY_ACTION_COMBO,       // Key combination (ctrl+c, alt+tab, etc.)
    KEY_ACTION_SEQUENCE,    // Sequence of keys
    KEY_ACTION_HOLD,        // Hold key for duration
    KEY_ACTION_TOGGLE       // Toggle key state
} KeyActionType;

// Key modifier flags
#define KEY_MOD_CTRL    (1 << 0)
#define KEY_MOD_ALT     (1 << 1)
#define KEY_MOD_SHIFT   (1 << 2)
#define KEY_MOD_SUPER   (1 << 3)  // Windows/Super key

typedef struct {
    int keycode;
    int modifiers;
    int duration_ms;  // For hold actions
} KeyAction;

typedef struct {
    KeyActionType type;
    KeyAction keys[8];  // Support up to 8 keys in combo/sequence
    int keyCount;
    char description[128];
} KeyPressAction;

// Hash table node for key mappings
typedef struct KeyHashNode {
    char name[MAX_KEY_NAME_LENGTH];
    int keycode;
    const char* description;
    struct KeyHashNode* next;  // For collision chaining
} KeyHashNode;

// Hash table structure
typedef struct {
    KeyHashNode* buckets[KEY_HASH_TABLE_SIZE];
    int size;  // Number of elements
} KeyHashTable;

// Core functions
int initKeyPressSystem(void);
void shutdownKeyPressSystem(void);
int executeKeyPress(const char* keyString);
int parseKeyString(const char* keyString, KeyPressAction* action);

// Hash table functions
unsigned int hashFunction(const char* key);
int initKeyHashTable(void);
void destroyKeyHashTable(void);
int insertKeyMapping(const char* name, int keycode, const char* description);
int getKeycodeFromName(const char* keyName);
const char* getKeyNameFromCode(int keycode);
void printHashTableStats(void);

// Key sending functions
int sendSingleKey(int keycode);
int sendKeyCombo(const KeyAction* keys, int keyCount);
int sendKeySequence(const KeyAction* keys, int keyCount);
int sendKeyHold(int keycode, int duration_ms);

// Utility functions
void listAvailableKeys(void);
void listKeyPressExamples(void);

// Built-in key press actions
int executeBuiltinKeyAction(const char* actionName, const char* parameter);

extern int keyPressSystemInitialized;
extern int keyPressDebugEnabled;
extern KeyHashTable* keyHashTable;

#endif
